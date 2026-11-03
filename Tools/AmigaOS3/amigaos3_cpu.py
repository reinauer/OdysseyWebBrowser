"""Code generation profiles shared by the AmigaOS 3 build scripts.

Every object in one link must use the same profile. With -m68881 the compiler
returns float and double results in fp0 rather than d0/d1, and the SDK selects
its libm020/libm881 libraries, so mixing profiles silently breaks the ABI.
"""

PROFILES = {
    # 68020 to 68060 with a 68881/68882 or an on-chip FPU. -m68020-60 avoids the
    # 64-bit multiply and divide forms which the 68060 emulates in software.
    'fpu': ('-m68020-60', '-m68881'),
    # The recorded October 29 profile: 68020 code with software floating point.
    'soft': ('-m68020',),
}
DEFAULT = 'fpu'

# Explicit helper objects which replace defective SDK software floating point.
SOFT_FLOAT_HELPERS = ('floatunsidf.o', 'floatunsisf.o', 'truncdfsf2.o', 'double-math.o')


def add_argument(parser):
    parser.add_argument('--cpu', choices=sorted(PROFILES), default=DEFAULT,
                        help='code generation profile (default: %(default)s); '
                             'the runtime, ICU, dependencies and browser must match')


def flags(profile):
    return list(PROFILES[profile])


def flag_string(profile):
    return ' '.join(PROFILES[profile])


def soft_float(profile):
    return profile == 'soft'


def math_helpers(profile):
    return SOFT_FLOAT_HELPERS if soft_float(profile) else ()
