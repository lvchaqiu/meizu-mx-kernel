/*
 *
 * (C) COPYRIGHT 2010-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



/* NOTE: This is a copy of mali_cutils_cstr.c, suitable for kernel space,
 *       and with floating point support removed.
 */

/**
 * Set to 1 to compile this code in user space.
 *
 * Kernel space supports cutils_cstr_snprintf and cutils_cstr_vsnprintf. Note that kernel space
 * requires osk_divmod6432() in order to format 64-bit numbers.
 */
#define CUTILS_SUPPORT_USERSPACE	0 

#if CUTILS_SUPPORT_USERSPACE
#include <cutils/cstr/mali_cutils_cstr.h>
#include <cdbg/mali_cdbg_assert.h>
#include <stdlib/mali_stdlib.h>
#include <base/mali_base_debug.h>
#include <stddef.h>
#include <string.h>
#else

#include <osk/mali_osk.h>
#include <malisw/mali_malisw.h>

STATIC u32 mystrlen(const char* str);

#define BDBG_INTERNAL_ASSERT(a) OSK_INTERNAL_ASSERT(a)
#endif

/*
 * NOTE: IMPORTANT: Most functions in this file cannot use CDBG or BDBG
 * asserts, apart from BDBG_INTERNAL_ASSERT() and CDBG_COMPILE_ASSERT.
 * This is to prevent re-entrancy into CDBG/BDBG via Base Portable Debug
 * Channels, which makes use of functions in this file.
 *
 * For the few functions that are allowed use of CDBG/BDBG macros, they are
 * commented as such. In the absence of a comment, assume that it is not safe
 * to use BDBG/CDBG macros.
 */

/**
 * @addtogroup oskdebug Debug
 * @{
 * @addtogroup oskdebug_private Private
 * @{
 */

STATIC const u32 flag_alternate = 0x01;   /**< printf flag # */
STATIC const u32 flag_zero_pad  = 0x02;   /**< printf flag 0 */
STATIC const u32 flag_left      = 0x04;   /**< printf flag - */
STATIC const u32 flag_space     = 0x08;   /**< printf flag ' ' */
STATIC const u32 flag_sign      = 0x10;   /**< printf flag + */

/**
 * @brief Internal state for @c cutilsp_cstr_format_string
 */
typedef struct cutilsp_cstr_format_state
{
	/** Total number of characters that have passed through */
	size_t written;
	/** Callback function given to @c cutilsp_cstr_format_string */
	s32 (*output)(char ch, void *arg);
	/** User argument to be passed to @c output */
	void *arg;
} cutilsp_cstr_format_state;

/**
 * @brief Modifier characters for cutilsp_cstr_format_string
 */
typedef enum
{
	modifier_none,
	modifier_hh,
	modifier_h,
	modifier_l,
	modifier_ll,
	modifier_z,
	modifier_t
} cutilsp_cstr_format_modifier;

typedef struct cutilsp_cstr_formatter
{
	char *str;
	size_t space;
} cutilsp_cstr_formatter;

/**
 * @brief Output a single character from string formatting.
 *
 * @param [in, out] fss The callback state
 * @param [in] c        The character to output
 *
 * @return 0 on success, negative value on failure
 */
STATIC s32 cutilsp_cstr_emit_char(cutilsp_cstr_format_state *fss, char c)
{
	s32 status = fss->output(c, fss->arg);
	if (status < 0) return status;
	fss->written++;
	return 0;
}

/**
 * @brief Output a sequence of characters from string formatting
 *
 * @param [in, out] fss The callback state
 * @param [in] len      The length of @c s
 * @param [in] s        The string to output (need not be null-terminated)
 *
 * @return zero on success, negative value on failure
 */
STATIC s32 cutilsp_cstr_emit_chars(cutilsp_cstr_format_state *fss, size_t len, const char *s)
{
	size_t i;
	for (i = 0; i < len; i++)
	{
		s32 status = cutilsp_cstr_emit_char(fss, s[i]);
		if (status < 0) return status;
	}
	return 0;
}

/**
 * @brief Output a single character multiple times
 *
 * @param [in, out] fss    The callback state
 * @param [in] repeat      The number of times to output the character (may be zero)
 * @param [in] c           The character to output (may be null)

 * @return zero on success, negative value on failure
 */
STATIC s32 cutilsp_cstr_emit_char_repeat(cutilsp_cstr_format_state *fss, size_t repeat, char c)
{
	size_t i;
	for (i = 0; i < repeat; i++)
	{
		s32 status = cutilsp_cstr_emit_char(fss, c);
		if (status < 0) return status;
	}
	return 0;
}

/**
 * @brief Output a number from string formatting
 *
 * @param [in, out] fss       The callback state
 * @param [in] prefix         An optional prefix to output before the number (e.g. sign)
 * @param [in] digits_len     The number of characters in digits
 * @param [in] digits         The actual digits of the number
 * @param [in] flags          The format flags (only @c flag_left and @c flag_zero_pad matter)
 * @param [in] width          The total field width, including prefix
 * @param [in] precision      The minimum number of digits to output
 * @param [in] trailing_zeros Number of extra zeros to output after the number itself
 * 
 * @pre At most one of @c flag_left and @c flag_zero_pad may be set
 * @pre precision must be non-negative
 */
STATIC s32 cutilsp_cstr_emit_number(cutilsp_cstr_format_state *fss, const char *prefix, size_t digits_len,
	const char *digits, u32 flags, s32 width, s32 precision, s32 trailing_zeros)
{
	const s32 prefix_len = mystrlen(prefix);
	s32 total_len = prefix_len + digits_len + trailing_zeros;
	s32 extra_digits = 0;
	s32 pad = 0;
	s32 status;

	if (digits_len < precision)
	{
		extra_digits = precision - digits_len;
		total_len += extra_digits;
	}

	if (total_len < width)
	{
		if (flags & flag_zero_pad)
		{
			extra_digits += width - total_len;
		}
		else
		{
			pad = width - total_len;
			if (!(flags & flag_left))
			{
				status = cutilsp_cstr_emit_char_repeat(fss, pad, ' ');
				if (status < 0) return status;
			}
		}
	}

	status = cutilsp_cstr_emit_chars(fss, prefix_len, prefix);
	if (status < 0) return status;
	status = cutilsp_cstr_emit_char_repeat(fss, extra_digits, '0');
	if (status < 0) return status;
	status = cutilsp_cstr_emit_chars(fss, digits_len, digits);
	if (status < 0) return status;
	status = cutilsp_cstr_emit_char_repeat(fss, trailing_zeros, '0');
	if (status < 0) return status;

	if (flags & flag_left)
	{
		status = cutilsp_cstr_emit_char_repeat(fss, pad, ' ');
		if (status < 0) return status;
	}
	return 0;
}

STATIC s32 cutilsp_cstr_format_d(cutilsp_cstr_format_state *fss, u32 flags, s32 width, s32 precision, s64 value)
{
	mali_bool neg = MALI_FALSE;
	mali_bool max_neg = MALI_FALSE;

	/* Buffer to hold the formatted number. It is filled in backwards
	 * from the end.
	 */
	char buffer[22];
	char *buffer_end = buffer + sizeof(buffer) - 1;
	char *buffer_ptr;
	const char *prefix;

	if (precision >= 0)
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	if (value < 0)
	{
		neg = MALI_TRUE;
		/* Handle overflow. Assumes two's complement */
		if (value << 1 == 0)
		{
			max_neg = MALI_TRUE;
			value++;
		}
		value = -value;
	}

	*buffer_end = '\0';
	buffer_ptr = buffer_end;
	while (value > 0)
	{
#if CUTILS_SUPPORT_USERSPACE
		*--buffer_ptr = (char) ('0' + (value % 10));
		value /= 10;
#else
		*--buffer_ptr = (char) ('0' + (osk_divmod6432(&value, 10)));
#endif

	}
	if (max_neg)
	{
		/* We decreased the value by one. This can only happen for a power
		 * of two, so we don't need to worry about carry when correcting this.
		 */
		buffer_end[-1]++;
	}
	if (neg)
		prefix = "-";
	else if (flags & flag_sign)
		prefix = "+";
	else if (flags & flag_space)
		prefix = " ";
	else
		prefix = "";
	return cutilsp_cstr_emit_number(fss, prefix,
									 buffer_end - buffer_ptr, buffer_ptr,
									 flags, width, precision, 0);
}

STATIC s32 cutilsp_cstr_format_u(cutilsp_cstr_format_state *fss,
								  u32 flags, s32 width, s32 precision,
								  u64 value)
{
	/* Buffer to hold the formatted number. It is filled in backwards
	 * from the end.
	 */
	char buffer[22];
	char *buffer_end = buffer + sizeof(buffer) - 1;
	char *buffer_ptr;

	if (precision >= 0)
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	*buffer_end = '\0';
	buffer_ptr = buffer_end;
	while (value > 0)
	{
#if CUTILS_SUPPORT_USERSPACE
		*--buffer_ptr = (char) ('0' + (value % 10));
		value /= 10;
#else
		*--buffer_ptr = (char) ('0' + (osk_divmod6432(&value, 10)));
#endif
	}
	return cutilsp_cstr_emit_number(fss, "",
									 buffer_end - buffer_ptr, buffer_ptr,
									 flags, width, precision, 0);
}

/**
 * @brief Format an octal or hex number, or a pointer value
 */
STATIC s32 cutilsp_cstr_format_binary(cutilsp_cstr_format_state *fss,
									   const char *prefix,
									   mali_bool prefix_zero,
									   s32 shift,
									   const char *char_table,
									   u32 flags,
									   s32 width, s32 precision,
									   u64 value)
{
	u32 mask = (1U << shift) - 1;

	/* Buffer to hold the formatted number. It is filled in backwards
	 * from the end.
	 */
	char buffer[65];   /* big enough for binary 64-bit in future */
	char *buffer_end = buffer + sizeof(buffer) - 1;
	char *buffer_ptr;

	*buffer_end = '\0';
	buffer_ptr = buffer_end;
	if (value == 0 && !prefix_zero)
	{
		prefix = "";
	}
	while (value > 0)
	{
		*--buffer_ptr = char_table[value & mask];
		value >>= shift;
	}
	return cutilsp_cstr_emit_number(fss, prefix,
									 buffer_end - buffer_ptr, buffer_ptr,
									 flags, width, precision, 0);
}

STATIC s32 cutilsp_cstr_format_o(cutilsp_cstr_format_state *fss,
								  u32 flags,
								  s32 width, s32 precision,
								  u64 value)
{
	const char *prefix = "";
	STATIC const char char_table[] = "01234567";

	if (precision >= 0)
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	if (flags & flag_alternate)
	{
		prefix = "0";
		/* This prefix counts against the precision, so we need to reduce
		 * the precision such that "%#.3o", 0 produces 000 not 0000.
		 */
		if (precision > 0) precision--;
	}

	return cutilsp_cstr_format_binary(fss, prefix, MALI_TRUE, 3,
									   char_table, flags, width, precision,
									   value);
}

STATIC s32 cutilsp_cstr_format_hex(cutilsp_cstr_format_state *fss,
									mali_bool upper_case,
									mali_bool prefix_zero,
									u32 flags,
									s32 width, s32 precision,
									u64 value)
{
	const char *prefix = "";
	const char *char_table = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";

	if (precision >= 0)
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	if (flags & flag_alternate)
	{
		prefix = upper_case ? "0X" : "0x";
	}

	return cutilsp_cstr_format_binary(fss, prefix, prefix_zero, 4,
									   char_table, flags, width, precision,
									   value);
}

STATIC s32 cutilsp_cstr_format_p(cutilsp_cstr_format_state *fss,
								  u32 flags,
								  s32 width, s32 precision,
								  u64 value)
{
	/* Force the 0x prefix */
	flags |= flag_alternate;

	/* Default to a full-width output for the size of pointers */
	if (precision >= 0)
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = sizeof(void *) * 2; /* Two hex digits per byte */
	}

	return cutilsp_cstr_format_hex(fss, MALI_FALSE, MALI_TRUE, flags, width, precision, value);
}

STATIC s32 cutilsp_cstr_format_s(cutilsp_cstr_format_state *fss, u32 flags,
								  s32 width, s32 precision, const char *str)
{
	size_t len;
	s32 status;

	if (precision >= 0)
	{
		size_t i;
		len = precision;

		/* Check whether the string is actually shorter than len. We cannot
		 * use strlen because the string is not required to be
		 * null-terminated in this case.
		 */
		for (i = 0; i < len; i++)
			if (str[i] == '\0')
			{
				len = i;
				break;
			}
	}
	else
	{
		len = mystrlen(str); 
	}

	if (width >= 0 && (size_t) width > len && !(flags & flag_left))
	{
		/* We ignore flag_zero, since it's undefined with %s anyway */
		status = cutilsp_cstr_emit_char_repeat(fss, width - len, ' ');
		if (status < 0) return status;
	}
	status = cutilsp_cstr_emit_chars(fss, len, str);
	if (status < 0) return status;
	if (width >= 0 && (size_t) width > len && (flags & flag_left))
	{
		status = cutilsp_cstr_emit_char_repeat(fss, width - len, ' ');
		if (status < 0) return status;
	}
	return 0;
}

STATIC s32 cutilsp_cstr_format_c(cutilsp_cstr_format_state *fss, u32 flags,
								  s32 width, char ch)
{
	s32 status;

	if (width > 1 && !(flags & flag_left))
	{
		/* We ignore flag_zero, since it's undefined with %s anyway */
		status = cutilsp_cstr_emit_char_repeat(fss, width - 1, ' ');
		if (status < 0) return status;
	}

	status = cutilsp_cstr_emit_char(fss, ch);
	if (status < 0) return status;

	if (width > 1 && (flags & flag_left))
	{
		status = cutilsp_cstr_emit_char_repeat(fss, width - 1, ' ');
		if (status < 0) return status;
	}

	return 0;
}

STATIC u32 cutilsp_cstr_parse_flags(const char **format)
{
	u32 flags = 0;

	while (1)
	{
		switch (**format)
		{
		case '#': flags |= flag_alternate; break;
		case '0': flags |= flag_zero_pad;  break;
		case '-': flags |= flag_left;      break;
		case ' ': flags |= flag_space;     break;
		case '+': flags |= flag_sign;      break;
		default:  return flags;
		}
		++*format;
	}
}

/* Parse the width field (possibly empty) from a format string.
 * Returns true if no width was present or one was parsed, false
 * if the width was '*'. If no width was present, *width is set to 0.
 */
STATIC mali_bool cutilsp_cstr_parse_width(const char **format, s32 *width)
{
	char next;

	next = **format;
	/* Parse field width */
	if (next == '*')
	{
		++*format;
		return MALI_FALSE;
	}
	else
	{
		s32 w = 0;
		/* Don't need to worry about a negative width here, since the
		 * - would have been treated as a flag.
		 */
		while (next >= '0' && next <= '9')
		{
			w = w * 10L + (next - '0');
			++*format;
			next = **format;
		}
		*width = w;
		return MALI_TRUE;
	}
}

/* Parses the precision field (possibly absent), including the '.', from
 * a format string. Returns true if the precision was absent, or was found and
 * parsed. Returns false if the precision was '*'.
 * If the precision was absent, *precision is set to a negative value.
 */
STATIC mali_bool cutilsp_cstr_parse_precision(const char **format, s32 *precision)
{
	char next;

	next = **format;
	if (next == '.')
	{
		++*format;
		next = **format;
		if (next == '*')
		{
			++*format;
			return MALI_FALSE;
		}
		else
		{
			mali_bool neg = MALI_FALSE;
			s32 p = 0; /* C99: precision of just '.' is same as 0 */

			if (next == '-')
			{
				neg = MALI_TRUE;
				++*format;
				next = **format;
			}
			while (next >= '0' && next <= '9')
			{
				p = p * 10L + (next - '0');
				++*format;
				next = **format;
			}
			if (neg) p = -p;
			*precision = p;
			return MALI_TRUE;
		}
	}
	else
	{
		*precision = -1;
		return MALI_TRUE;
	}
}

/* Parses a (possibly absent) length modifier from a format string. */
STATIC cutilsp_cstr_format_modifier cutilsp_cstr_parse_modifier(const char **format)
{
	char next;

	next = **format;
	switch (next)
	{
	case 'h':
		++*format;
		next = **format;
		if (next == 'h')
		{
			++*format;
			return modifier_hh;
		}
		return modifier_h;
	case 'l':
		++*format;
		next = **format;
		if (next == 'l')
		{
			++*format;
			return modifier_ll;
		}
		return modifier_l;
	case 'z':
		++*format;
		return modifier_z;
	case 't':
		++*format;
		return modifier_t;
	default:
		return modifier_none;
	}
}

/**
 * @brief printf-style string formatting.
 *
 * Refer to the specification for restrictions on the format string.
 *
 * @param output A callback function that is fed each character. It must
 *               return a non-negative value on success, negative on failure.
 * @param arg    A generic argument passed as the second argument to @c output
 * @param format The format string
 * @param ap The variadic arguments to format.
 *
 * @return The number of characters written on success, or a negative value
 * on failure.
 */
STATIC s32 cutilsp_cstr_format_string(s32 (*output)(char, void *), void *arg, const char *format, va_list ap)
{
	s32 status;
	cutilsp_cstr_format_state fss;

	fss.written = 0;
	fss.output = output;
	fss.arg = arg;

	while (*format != '\0')
	{
		char next = *format++;
		if (next != '%')
		{
			status = cutilsp_cstr_emit_char(&fss, next);
			if (status < 0) return status;
		}
		else if (*format == '%')
		{
			format++;
			status = cutilsp_cstr_emit_char(&fss, '%');
			if (status < 0) return status;
		}
		else
		{
			u32 flags;
			s32 width, precision;
			cutilsp_cstr_format_modifier modifier;

			flags = cutilsp_cstr_parse_flags(&format);
			if (!cutilsp_cstr_parse_width(&format, &width))
				width = va_arg(ap, s32);
			if (!cutilsp_cstr_parse_precision(&format, &precision))
				precision = va_arg(ap, s32);
			modifier = cutilsp_cstr_parse_modifier(&format);

			/* C99 seems to imply this happens even for asterisk form */
			if (width < 0)
			{
				flags |= flag_left;
				width = -width;
			}
			/* C99 specifies that some flags override others */
			if (flags & flag_sign)
				flags &= ~flag_space;
			if (flags & flag_left)
				flags &= ~flag_zero_pad;

			/* Parse conversion specifier. We also have to deal with all
			 * combinations of length modifier and conversion specifier
			 * in this function, since there is no reliable portable way
			 * to pass off the va_list and then continue to use it here
			 * (in spite of what C99 promises).
			 */
			next = *format++;
			switch (next)
			{
			case 'd':
			case 'i':
				{
					s64 value;
					switch (modifier)
					{
					case modifier_hh:
						value = (s64) (signed char) va_arg(ap, int);
						break;
					case modifier_h:
						value = (s64) (short int) va_arg(ap, int);
						break;
					case modifier_none:
						value = va_arg(ap, int);
						break;
					case modifier_l:
						value = va_arg(ap, long int);
						break;
					case modifier_ll:
						value = va_arg(ap, s64);
						break;
					case modifier_t:
						value = va_arg(ap, ptrdiff_t);
						break;
					default:
						return -1;
					}

					status = cutilsp_cstr_format_d(&fss, flags, width, precision, value);
					if (status < 0) return status;
				}
				break;
			case 'o':
			case 'u':
			case 'x':
			case 'X':
				{
					u64 value;
					switch (modifier)
					{
					case modifier_hh:
						value = (u64) (unsigned char) va_arg(ap, int);
						break;
					case modifier_h:
						value = (u64) (unsigned short int) va_arg(ap, int);
						break;
					case modifier_none:
						value = va_arg(ap, u32);
						break;
					case modifier_l:
						value = va_arg(ap, unsigned long int);
						break;
					case modifier_ll:
						value = va_arg(ap, u64);
						break;
					case modifier_z:
						value = va_arg(ap, size_t);
						break;
					default:
						return -1;
					}

					switch (next)
					{
					case 'u':
						status = cutilsp_cstr_format_u(&fss, flags, width, precision, value);
						break;
					case 'o':
						status = cutilsp_cstr_format_o(&fss, flags, width, precision, value);
						break;
					case 'x':
						status = cutilsp_cstr_format_hex(&fss, MALI_FALSE, MALI_FALSE, flags, width, precision, value);
						break;
					case 'X':
						status = cutilsp_cstr_format_hex(&fss, MALI_TRUE, MALI_FALSE, flags, width, precision, value);
						break;
					default:
						BDBG_INTERNAL_ASSERT(0); /* Should not be reachable */
						return -1;
					}
					if (status < 0) return status;
				}
				break;
			case 'p':
				{
					u64 value;

					if (modifier != modifier_none)
					{
						return -1;
					}
					value = (u64) (size_t) va_arg(ap, void *);
					status = cutilsp_cstr_format_p(&fss, flags, width, precision, value);
					if (status < 0) return status;
				}
				break;
			case 'n':
				{
					switch (modifier)
					{
					case modifier_ll:
						*(va_arg(ap, s64 *)) = fss.written;
						break;
					case modifier_l:
						*(va_arg(ap, long *)) = fss.written;
						break;
					case modifier_t:
						*(va_arg(ap, ptrdiff_t *)) = fss.written;
						break;
					case modifier_z:
						*(va_arg(ap, size_t *)) = fss.written;
						break;
					case modifier_none:
						*(va_arg(ap, int *)) = fss.written;
						break;
					default:
						return -1;
					}
				}
				break;
			case 's':
				if (modifier != modifier_none)
				{
					return -1;
				}
				status = cutilsp_cstr_format_s(&fss, flags, width, precision, va_arg(ap, const char *));
				if (status < 0) return status;
				break;
			case 'c':
				if (modifier != modifier_none)
				{
					return -1;
				}
				status = cutilsp_cstr_format_c(&fss, flags, width, va_arg(ap, int));
				if (status < 0) return status;
				break;
			default:
				return -1;  /* Invalid specifier */
			}
		}
	}
	return fss.written;
}

/**
 * @brief Write one character to a buffer of characters
 *
 * @param [in] ch Character to write
 * @param [in, out] arg Buffer of character of type cutils
 */
STATIC s32 cutilsp_cstr_append(char ch, void *arg)
{
	cutilsp_cstr_formatter *sf = (cutilsp_cstr_formatter *) arg;
	if (sf->space > 1)
	{
		*sf->str++ = ch;
		sf->space--;
	}
	return (unsigned char) ch;
}

/**
 * @}
 * @}
 */
/* End String formatting Private group */
/* End String formatting group */

/************************************ Implementation of the public functions *****************************************/

/* This function is NOT allowed to call most CDBG/BDBG macros - see top of
 * file. */
s32 cutils_cstr_vsnprintf( char *str, 
		           size_t size, 
			   const char *format, 
			   va_list ap)
{
	cutilsp_cstr_formatter sf;
	s32 ret;

	sf.str = str; sf.space = size;

	ret = cutilsp_cstr_format_string(cutilsp_cstr_append, &sf, format, ap);
	if (sf.space > 0) *sf.str = '\0';
	return ret;
}

/* This function is NOT allowed to call most CDBG/BDBG macros - see top of
 * file. */
s32 cutils_cstr_snprintf( char *str, 
		          size_t size, 
			  const char *format, 
			  ...)
{
	cutilsp_cstr_formatter sf;
	s32 ret;
	va_list ap;

	sf.str = str; sf.space = size;

	va_start(ap, format);
	ret = cutilsp_cstr_format_string(cutilsp_cstr_append, &sf, format, ap);
	va_end(ap);
	if (sf.space > 0) *sf.str = '\0';
	return ret;
}

/* Provided to prevent dependency on C library */
STATIC u32 mystrlen(const char* str)
{
	const char *s;
	for(s = str; *s != '\0'; s++)
	{
		;
	}
	return (s - str);
}
