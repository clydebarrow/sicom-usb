#!/usr/bin/env python3
from email.policy import default

import yaml
import sys
import io
import voluptuous as cv
from esphome.components.xiaomi_hhccjcy01.sensor import xiaomi_hhccjcy01_ns
from ruamel.yaml.compat import ordereddict


def int_(value):
    """Validate that the config option is an integer.

    Automatically also converts strings to ints.
    """
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        if int(value) == value:
            return int(value)
        raise cv.Invalid(
            f"This option only accepts integers with no fractional part. Please remove the fractional part from {value}"
        )
    value = str(value).lower()
    base = 10
    if value.startswith("0x"):
        base = 16
    try:
        return int(value, base)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(f"Expected integer, but cannot parse {value} as an integer")


def int_range(min=None, max=None, min_included=True, max_included=True):
    """Validate that the config option is an integer in the given range."""
    if min is not None:
        assert isinstance(min, int)
    if max is not None:
        assert isinstance(max, int)
    return cv.All(
        int_,
        cv.Range(min=min, max=max, min_included=min_included, max_included=max_included),
    )


def string(value):
    """Validate that a configuration value is a string. If not, automatically converts to a string.

    Note that this can be lossy, for example the input value 60.00 (float) will be turned into
    "60.0" (string). For values where this could be a problem `string_strict` has to be used.
    """
    if isinstance(value, (dict, list)):
        raise cv.Invalid("string value cannot be dictionary or list.")
    if isinstance(value, bool):
        raise cv.Invalid(
            "Auto-converted this value to boolean, please wrap the value in quotes."
        )
    if isinstance(value, str):
        return value
    if value is not None:
        return str(value)
    raise cv.Invalid("string value is None")


def string_strict(value):
    """Like string, but only allows strings, and does not automatically convert other types to
    strings."""
    if isinstance(value, str):
        return value
    raise cv.Invalid(
        f"Must be string, got {type(value)}. did you forget putting quotes around the value?"
    )


def float_(value):
    return cv.Coerce(float)(value)


def one_of(*values, **kwargs):
    """Validate that the config option is one of the given values.

    :param values: The valid values for this type

    :Keyword Arguments:
      - *lower* (``bool``, default=False): Whether to convert the incoming values to lowercase
        strings.
      - *upper* (``bool``, default=False): Whether to convert the incoming values to uppercase
        strings.
      - *int* (``bool``, default=False): Whether to convert the incoming values to integers.
      - *float* (``bool``, default=False): Whether to convert the incoming values to floats.
      - *space* (``str``, default=' '): What to convert spaces in the input string to.
    """
    options = ", ".join(f"'{x}'" for x in values)
    lower = kwargs.pop("lower", False)
    upper = kwargs.pop("upper", False)
    string_ = kwargs.pop("string", False) or lower or upper
    to_int = kwargs.pop("int", False)
    to_float = kwargs.pop("float", False)
    space = kwargs.pop("space", " ")
    if kwargs:
        raise ValueError

    def validator(value):
        if string_:
            value = string(value)
            value = value.replace(" ", space)
        if to_int:
            value = int_(value)
        if to_float:
            value = float_(value)
        if lower:
            value = cv.Lower(value)
        if upper:
            value = cv.Upper(value)
        if value not in values:
            import difflib

            options_ = [str(x) for x in values]
            option = str(value)
            matches = difflib.get_close_matches(option, options_)
            if matches:
                matches_str = ", ".join(f"'{x}'" for x in matches)
                raise cv.Invalid(f"Unknown value '{value}', did you mean {matches_str}?")
            raise cv.Invalid(f"Unknown value '{value}', valid options are {options}.")
        return value

    return validator


def boolean(value):
    """Validate the given config option to be a boolean.

    This option allows a bunch of different ways of expressing boolean values:
     - instance of boolean
     - 'true'/'false'
     - 'yes'/'no'
     - 'enable'/disable
    """
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        value = value.lower()
        if value in ("true", "yes", "on", "enable"):
            return True
        if value in ("false", "no", "off", "disable"):
            return False
    raise cv.Invalid(
        f"Expected boolean value, but cannot convert {value} to a boolean. Please use 'true' or 'false'"
    )


def ensure_list(*validators):
    """Validate this configuration option to be a list.

    If the config value is not a list, it is automatically converted to a
    single-item list.

    None and empty dictionaries are converted to empty lists.
    """
    user = cv.All(*validators)
    list_schema = cv.Schema([user])

    def validator(value):
        if value is None or (isinstance(value, dict) and not value):
            return []
        if not isinstance(value, list):
            return [user(value)]
        return list_schema(value)

    return validator


def typed_schema(schemas, **kwargs):
    """Create a schema that has a key to distinguish between schemas"""
    key = kwargs.pop("key", "type")
    default_schema_option = kwargs.pop("default_type", None)
    key_validator = one_of(*schemas, **kwargs)

    def validator(value):
        if not isinstance(value, dict):
            raise cv.Invalid("Value must be dict")
        value = value.copy()
        schema_option = value.pop(key, default_schema_option)
        if schema_option is None:
            raise cv.Invalid(f"{key} not specified!")
        key_v = key_validator(schema_option)
        value = cv.Schema(schemas[key_v])(value)
        value[key] = key_v
        return value

    return validator


ascii_lowercase = 'abcdefghijklmnopqrstuvwxyz'
ascii_uppercase = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ascii_letters = ascii_lowercase + ascii_uppercase
digits = '0123456789'


def validate_id_name(value):
    """Validate that the given value would be a valid C++ identifier name."""
    value = string(value)
    if not value:
        raise cv.Invalid("ID must not be empty")
    if value[0].isdigit():
        raise cv.Invalid("First character in ID cannot be a digit.")
    if "-" in value:
        raise cv.Invalid(
            "Dashes are not supported in IDs, please use underscores instead."
        )

    valid_chars = f"{ascii_letters + digits}_"
    for char in value:
        if char not in valid_chars:
            raise cv.Invalid(
                f"IDs must only consist of upper/lowercase characters, the underscorecharacter and numbers. The character '{char}' cannot be used"
            )
    return value


def version_number(value):
    """Validate that the given value is a version number."""
    if not value:
        raise cv.Invalid("Version number must not be empty")
    value = string(value)
    if not all(x in digits + "." for x in value):
        raise cv.Invalid("Version number must only contain digits and dots.")
    if value.count(".") != 1:
        raise cv.Invalid("Version number must contain exactly one dot.")
    major, minor = value.split(".")
    major = int(major)
    minor = int(minor)
    if major < 0 or minor < 0:
        raise cv.Invalid("Version number must be positive.")
    if major > 255 or minor > 255:
        raise cv.Invalid("Version numbers must be less than 256.")
    return minor + (major << 8)


DEVICE_TYPE = 0x1
DEVICE_SIZE = 18
CONFIGURATION_TYPE = 0x2
CONFIGURATION_SIZE = 9
INTERFACE_TYPE = 0x4
INTERFACE_SIZE = 9
ENDPOINT_TYPE = 0x5
ENDPOINT_SIZE = 7
DFU_SIZE = 9

ENDPOINT_BASE = cv.Schema(
    {
        cv.Required("number"): int_range(min=0, max=15),
        cv.Required("direction"): one_of("in", "out", lower=True),
        cv.Required("max_packet_size"): one_of(8, 16, 32, 64),
        cv.Optional("interval"): int_range(min=0, max=255),
    }
)

ENDPOINT_TYPES = ("control", "isochronous", "bulk", "interrupt")
SYNC_TYPES = ("none", "asynchronous", "adaptive", "synchronous")
USAGE_TYPES = ("data", "feedback", "explicit_feedback")

ENDPOINT_SCHEMA = typed_schema(
    {
        "bulk": ENDPOINT_BASE,
        "interrupt": ENDPOINT_BASE.extend({
            cv.Required("interval"): int_range(min=1, max=255),
        }),
        "isochronous": ENDPOINT_BASE.extend({
            cv.Optional("synchronization_type", default="none"): one_of(SYNC_TYPES, lower=True),
            cv.Optional("usage_type", default="data"): one_of(USAGE_TYPES, lower=True),
        }),
    }
)

INTERFACE_SCHEMA = cv.Schema(
    {
        cv.Optional("name", default=""): string_strict,
        cv.Optional("class", default=0): int_range(min=0, max=0xFF),
        cv.Optional("subclass", default=0): int_range(min=0, max=0xFF),
        cv.Optional("protocol", default=0): int_range(min=0, max=0xFF),
        cv.Optional("alternate", default=0): int_range(min=0, max=0xFF),
        cv.Required("endpoints"): ensure_list(ENDPOINT_SCHEMA),
    }
)

CONFIGURATION_SCHEMA = cv.Schema(
    {
        cv.Optional("name", default=""): string_strict,
        cv.Optional("wakeup", default=False): boolean,
        cv.Optional("power_source", default="USB"): one_of("bus", "self", lower=True),
        cv.Optional("max_current", default=100): int_range(min=2, max=500),
        cv.Required("interfaces"): ensure_list(INTERFACE_SCHEMA),
    }
)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required("id"): validate_id_name,
        cv.Required("vendor_id"): int_range(min=0, max=0xFFFF),
        cv.Required("product_id"): int_range(min=0, max=0xFFFF),
        cv.Optional("max_packet_size_0", default=64): one_of(8, 16, 32, 64),
        cv.Required("usb_version"): one_of("1.0", "1.1", "2.0"),
        cv.Required("manufacturer"): string_strict,
        cv.Required("product"): string_strict,
        cv.Required("release"): version_number,
        cv.Optional("serial", default=""): string_strict,
        cv.Required("class"): int_range(min=0, max=0xFF),
        cv.Optional("subclass", default=0): int_range(min=0, max=0xFF),
        cv.Optional("protocol", default=0): int_range(min=0, max=0xFF),
        cv.Required("configurations"): ensure_list(CONFIGURATION_SCHEMA),
    }
)


class Writer():

    def __init__(self, file, ident):
        self.file = file
        self.indent = 0
        self.length = 0
        self.ident = ident

    def __enter__(self):
        self.fp = open(self.file, "w")
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.fp.close()

    def write(self, line, comment=None):
        self.fp.write(" " * self.indent + line)
        if comment:
            self.fp.write(" // " + comment)
        self.fp.write("\n")

    def start(self, typespec, name):
        self.write(typespec + " " + self.ident + "_" + name + " = {")
        self.increment()

    def end(self):
        self.decrement()
        self.write("};\n")

    def write_byte(self, byte, comment=None):
        if isinstance(byte, str):
            self.write(byte + ",", comment)
        else:
            self.write(f"0x{byte:02X},", comment)
        self.length += 1

    def write_word(self, word, comment=None):
        if isinstance(word, str):
            self.write(word + ",", comment)
        else:
            self.write(f"0x{(word & 0xFF):02X}, 0x{(word >> 8):02X},", comment + f" (0x{word:04X})")
        self.length += 2

    def write_array(self, array, comment=None):
        bstr = ", ".join(f"0x{byte:02X}" for byte in array)
        self.write(bstr + ",", comment)
        self.length += len(array)

    def newline(self):
        self.fp.write("\n")

    def increment(self):
        self.indent += 4

    def decrement(self):
        self.indent -= 4


class Stringtable():

    def __init__(self, ident):
        self.strings = {}
        self.data = []
        self.ident = ident
        self.addresses = [f"{self.ident}_string_data,"]
        self.position = 4

    def get(self, string):
        if not string:
            return 0
        if string not in self.strings:
            self.strings[string] = len(self.addresses)
            self.addresses.append(f"{self.ident}_string_data + {self.position},")
            self.position += 2 + len(string.encode("utf-16"))
        return self.strings[string]

    def write(self, os: Writer):
        os.newline()
        os.start("static USB_CONST unsigned char", "string_data[]")
        os.write("4, 3, 9, 4, // Language IDS")
        for string, index in self.strings.items():
            os.write(f"// {index}: '{string}'")
            os.increment()
            bytes = string.encode("utf-16")
            os.write_byte(len(bytes) + 2, "bLength")
            os.write_byte(0x03, "bDescriptorType")
            os.write_array(bytes, "bString")
            os.decrement()
        os.end()
        os.start("static USB_CONST unsigned char *USB_CONST", "strings[]")
        for address in self.addresses:
            os.write(address)
        os.end()

    def get_serial_index(self, serial):
        if serial:
            return self.get(serial)
        self.addresses.append("0, // Use bootload serial")
        return len(self.addresses) - 1

    @property
    def length(self):
        return len(self.strings) + 2


HEADER = """
#ifndef USB_CONST
#define  USB_CONST  const
#endif
    
#include  <htusb.h>
/* This structure is defined in htusb.h, here for reference only
    typedef struct{
        USB_CONST unsigned char *              device,
                                * USB_CONST *  configurations,
                                * USB_CONST *  strings;
        USB_CONST unsigned short*               conf_sizes;
        unsigned char              conf_cnt, string_cnt;
    }  USB_descriptor_table;
);
*/
""".strip()


def write_device(config, os, stringtable):
    os.write_byte(DEVICE_SIZE, "bLength")
    os.write_byte(DEVICE_TYPE, "bDescriptorType")
    usb_version = config['usb_version'].split(".")
    os.write_word(int(usb_version[0]) * 256 + int(usb_version[1]) * 16, "bcdUSB")
    os.write_byte(config['class'], "bDeviceClass")
    os.write_byte(config['subclass'], "bDeviceSubClass")
    os.write_byte(config['protocol'], "bDeviceProtocol")
    os.write_byte(config['max_packet_size_0'], "bMaxPacketSize0")
    os.write_word(config['vendor_id'], "idVendor")
    os.write_word(config['product_id'], "idProduct")
    os.write_word(config['release'], "bcdDevice")
    manu_string = stringtable.get(config['manufacturer'])
    os.write_byte(manu_string, "iManufacturer")
    product_string = stringtable.get(config['product'])
    os.write_byte(product_string, "iProduct")
    serial_string = stringtable.get_serial_index(config['serial'])
    os.write_byte(serial_string, "iSerialNumber")
    os.write_byte(len(config['configurations']), "bNumConfigurations")


def write_config(config, index, os, stringtable):
    os.write(f"// Configuration {index}: {stringtable.ident}")
    os.increment()
    os.write_byte(CONFIGURATION_SIZE, "bLength")
    os.write_byte(CONFIGURATION_TYPE, "bDescriptorType")
    total_size = CONFIGURATION_SIZE
    for interface in config['interfaces']:
        total_size += len(interface['endpoints']) * ENDPOINT_SIZE + INTERFACE_SIZE
    os.write_word(total_size, "wTotalLength")
    os.write_byte(len(config['interfaces']), "bNumInterfaces")
    os.write_byte(index + 1, "bConfigurationValue")
    os.write_byte(stringtable.get(config['name']), f"iConfiguration '{config['name']}'")
    attributes = 0x80 if config['power_source'] == "bus" else 0
    if config['power_source'] == "self":
        attributes |= 0x40
    if config['wakeup']:
        attributes |= 0x20
    os.write_byte(attributes, "bmAttributes")
    os.write_byte(config['max_current'] // 2, "bMaxPower")
    for index, interface in enumerate(config['interfaces']):
        os.increment()
        os.write(f"// Interface {index}: '{interface['name']}'")
        os.write_byte(INTERFACE_SIZE, "bLength")
        os.write_byte(INTERFACE_TYPE, "bDescriptorType")
        os.write_byte(index, "bInterfaceNumber")
        os.write_byte(interface['alternate'], "bAlternateSetting")
        os.write_byte(len(interface['endpoints']), "bNumEndpoints")
        os.write_byte(interface['class'], "bInterfaceClass")
        os.write_byte(interface['subclass'], "bInterfaceSubClass")
        os.write_byte(interface['protocol'], "bInterfaceProtocol")
        os.write_byte(stringtable.get(config['name']), "iInterface")
        for endpoint in interface['endpoints']:
            os.increment()
            os.write(f" // Endpoint {endpoint['number']}: {endpoint['direction']}")
            os.write_byte(ENDPOINT_SIZE, "bLength")
            os.write_byte(ENDPOINT_TYPE, "bDescriptorType")
            address = endpoint['number'] | (0x80 if endpoint['direction'] == "in" else 0)
            os.write_byte(address, "bEndpointAddress")
            attributes = ENDPOINT_TYPES.index(endpoint['type'])
            if endpoint['type'] == "isochronous":
                attributes |= SYNC_TYPES.index(endpoint['synchronization_type']) << 2
                attributes |= USAGE_TYPES.index(endpoint['usage_type']) << 4
            os.write_byte(ENDPOINT_TYPES.index(endpoint['type']), "bmAttributes")
            os.write_word(endpoint['max_packet_size'], "wMaxPacketSize")
            os.write_byte(endpoint.get('interval', 0), "bInterval")
            os.decrement()
        os.decrement()
    os.decrement()
    return total_size


def to_code(config, os: Writer):
    os.write(HEADER)
    id = config["id"]
    stringtable = Stringtable(config["id"])
    os.start("static USB_CONST unsigned char", "data[]")
    write_device(config, os, stringtable)
    conf_sizes = [write_config(config, index, os, stringtable) for index, config in enumerate(config["configurations"])]
    os.end()
    stringtable.write(os)
    os.start("static USB_CONST unsigned short", "conf_sizes[]")
    for size in conf_sizes:
        os.write(f"{size},")
    os.end()
    os.start("static USB_CONST unsigned char *USB_CONST", "configurations[]")
    index = DEVICE_SIZE
    for size in conf_sizes:
        os.write(f"{os.ident}_data + {index},")
        index += size
    os.end()
    os.start("USB_CONST USB_descriptor_table", "table")
    os.write(f"{os.ident}_data,")
    os.write(f"{os.ident}_configurations,")
    os.write(f"{os.ident}_strings,")
    os.write(f"{os.ident}_conf_sizes,")
    os.write_byte(len(config["configurations"]), "conf_cnt")
    os.write_byte(stringtable.length, "string_cnt")
    os.end()


# Main program

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <input_file>.yaml <output_file>.c")
    sys.exit(1)

with io.open(sys.argv[1]) as stream:
    config = yaml.safe_load(stream)

config = CONFIG_SCHEMA(config)
with Writer(sys.argv[2], config["id"]) as stream:
    to_code(config, stream)
