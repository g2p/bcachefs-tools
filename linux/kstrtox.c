/*
 * Convert integer string representation to an integer.
 * If an integer doesn't fit into specified type, -E is returned.
 *
 * Integer starts with optional sign.
 * kstrtou*() functions do not accept sign "-".
 *
 * Radix 0 means autodetection: leading "0x" implies radix 16,
 * leading "0" implies radix 8, otherwise radix is 10.
 * Autodetection hints work after optional sign, but not before.
 *
 * If -E is returned, result is not touched.
 */
#include <errno.h>
#include <ctype.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "kstrtox.h"

#define KSTRTOX_OVERFLOW	(1U << 31)

const char *_parse_integer_fixup_radix(const char *s, unsigned int *base)
{
	if (*base == 0) {
		if (s[0] == '0') {
			if (_tolower(s[1]) == 'x' && isxdigit(s[2]))
				*base = 16;
			else
				*base = 8;
		} else
			*base = 10;
	}
	if (*base == 16 && s[0] == '0' && _tolower(s[1]) == 'x')
		s += 2;
	return s;
}

/*
 * Convert non-negative integer string representation in explicitly given radix
 * to an integer.
 * Return number of characters consumed maybe or-ed with overflow bit.
 * If overflow occurs, result integer (incorrect) is still returned.
 *
 * Don't you dare use this function.
 */
unsigned int _parse_integer(const char *s, unsigned int base, unsigned long long *p)
{
	unsigned long long res;
	unsigned int rv;
	int overflow;

	res = 0;
	rv = 0;
	overflow = 0;
	while (*s) {
		unsigned int val;

		if ('0' <= *s && *s <= '9')
			val = *s - '0';
		else if ('a' <= _tolower(*s) && _tolower(*s) <= 'f')
			val = _tolower(*s) - 'a' + 10;
		else
			break;

		if (val >= base)
			break;
		/*
		 * Check for overflow only if we are within range of
		 * it in the max base we support (16)
		 */
		if (unlikely(res & (~0ull << 60))) {
			if (res > ULLONG_MAX - val / base)
				overflow = 1;
		}
		res = res * base + val;
		rv++;
		s++;
	}
	*p = res;
	if (overflow)
		rv |= KSTRTOX_OVERFLOW;
	return rv;
}

static int _kstrtoull(const char *s, unsigned int base, unsigned long long *res)
{
	unsigned long long _res;
	unsigned int rv;

	s = _parse_integer_fixup_radix(s, &base);
	rv = _parse_integer(s, base, &_res);
	if (rv & KSTRTOX_OVERFLOW)
		return -ERANGE;
	if (rv == 0)
		return -EINVAL;
	s += rv;
	if (*s == '\n')
		s++;
	if (*s)
		return -EINVAL;
	*res = _res;
	return 0;
}

/**
 * kstrtoull - convert a string to an unsigned long long
 * @s: The start of the string. The string must be null-terminated, and may also
 *  include a single newline before its terminating null. The first character
 *  may also be a plus sign, but not a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 *  given as 0, then the base of the string is automatically detected with the
 *  conventional semantics - If it begins with 0x the number will be parsed as a
 *  hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 *  parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
int kstrtoull(const char *s, unsigned int base, unsigned long long *res)
{
	if (s[0] == '+')
		s++;
	return _kstrtoull(s, base, res);
}

/**
 * kstrtoll - convert a string to a long long
 * @s: The start of the string. The string must be null-terminated, and may also
 *  include a single newline before its terminating null. The first character
 *  may also be a plus sign or a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 *  given as 0, then the base of the string is automatically detected with the
 *  conventional semantics - If it begins with 0x the number will be parsed as a
 *  hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 *  parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
int kstrtoll(const char *s, unsigned int base, long long *res)
{
	unsigned long long tmp;
	int rv;

	if (s[0] == '-') {
		rv = _kstrtoull(s + 1, base, &tmp);
		if (rv < 0)
			return rv;
		if ((long long)-tmp > 0)
			return -ERANGE;
		*res = -tmp;
	} else {
		rv = kstrtoull(s, base, &tmp);
		if (rv < 0)
			return rv;
		if ((long long)tmp < 0)
			return -ERANGE;
		*res = tmp;
	}
	return 0;
}

/* Internal, do not use. */
int _kstrtoul(const char *s, unsigned int base, unsigned long *res)
{
	unsigned long long tmp;
	int rv;

	rv = kstrtoull(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (unsigned long long)(unsigned long)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

/* Internal, do not use. */
int _kstrtol(const char *s, unsigned int base, long *res)
{
	long long tmp;
	int rv;

	rv = kstrtoll(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (long long)(long)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

/**
 * kstrtouint - convert a string to an unsigned int
 * @s: The start of the string. The string must be null-terminated, and may also
 *  include a single newline before its terminating null. The first character
 *  may also be a plus sign, but not a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 *  given as 0, then the base of the string is automatically detected with the
 *  conventional semantics - If it begins with 0x the number will be parsed as a
 *  hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 *  parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
int kstrtouint(const char *s, unsigned int base, unsigned int *res)
{
	unsigned long long tmp;
	int rv;

	rv = kstrtoull(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (unsigned long long)(unsigned int)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

/**
 * kstrtoint - convert a string to an int
 * @s: The start of the string. The string must be null-terminated, and may also
 *  include a single newline before its terminating null. The first character
 *  may also be a plus sign or a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 *  given as 0, then the base of the string is automatically detected with the
 *  conventional semantics - If it begins with 0x the number will be parsed as a
 *  hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 *  parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
int kstrtoint(const char *s, unsigned int base, int *res)
{
	long long tmp;
	int rv;

	rv = kstrtoll(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (long long)(int)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

int kstrtou16(const char *s, unsigned int base, u16 *res)
{
	unsigned long long tmp;
	int rv;

	rv = kstrtoull(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (unsigned long long)(u16)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

int kstrtos16(const char *s, unsigned int base, s16 *res)
{
	long long tmp;
	int rv;

	rv = kstrtoll(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (long long)(s16)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

int kstrtou8(const char *s, unsigned int base, u8 *res)
{
	unsigned long long tmp;
	int rv;

	rv = kstrtoull(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (unsigned long long)(u8)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

int kstrtos8(const char *s, unsigned int base, s8 *res)
{
	long long tmp;
	int rv;

	rv = kstrtoll(s, base, &tmp);
	if (rv < 0)
		return rv;
	if (tmp != (long long)(s8)tmp)
		return -ERANGE;
	*res = tmp;
	return 0;
}

/**
 * kstrtobool - convert common user inputs into boolean values
 * @s: input string
 * @res: result
 *
 * This routine returns 0 iff the first character is one of 'Yy1Nn0', or
 * [oO][NnFf] for "on" and "off". Otherwise it will return -EINVAL.  Value
 * pointed to by res is updated upon finding a match.
 */
int kstrtobool(const char *s, bool *res)
{
	if (!s)
		return -EINVAL;

	switch (s[0]) {
	case 'y':
	case 'Y':
	case '1':
		*res = true;
		return 0;
	case 'n':
	case 'N':
	case '0':
		*res = false;
		return 0;
	case 'o':
	case 'O':
		switch (s[1]) {
		case 'n':
		case 'N':
			*res = true;
			return 0;
		case 'f':
		case 'F':
			*res = false;
			return 0;
		default:
			break;
		}
	default:
		break;
	}

	return -EINVAL;
}
