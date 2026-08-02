# decode_settings.py
# Temporary diagnostic helper: decode JUCE MemoryBlock::toBase64Encoding() stored in the
# standalone PropertiesFile (.settings) into the XML produced by getStateInformation(),
# then print the A/B slot attributes so the slot-persistence bug can be localised.
#
# Format (from JUCE MemoryBlock::toBase64Encoding / copyXmlToBinary):
#   "N." + bit-packed 6-bit groups over the alphabet ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
#   binary = uint32 magic (0x21324356) + uint32 textLen + UTF-8 XML text (single line) + NUL

import re
import sys
import binascii
import xml.etree.ElementTree as ET

ALPHABET = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
INDEX = {c: i for i, c in enumerate(ALPHABET)}

MAGIC = 0x21324356


def extract_value(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()
    m = re.search(r'<VALUE name="filterState" val="([^"]*)"', content)
    if not m:
        print("ERROR: filterState not found")
        sys.exit(1)
    return m.group(1)


def decode_bitpacked(s):
    # s starts with "N." prefix: total byte size followed by the 6-bit chars.
    # JUCE writes 6-bit groups over the whole memory as a bit stream where bit
    # position p maps to byte (p>>3), bit (p&7) (LSB-first), see
    # MemoryBlock::getBitRange(). Reconstruct exactly the same way.
    dot = s.index(".")
    size = int(s[:dot])
    chars = s[dot + 1:]
    groups = []
    for ch in chars:
        v = INDEX.get(ch)
        if v is None:
            print("ERROR: unexpected char %r" % ch)
            sys.exit(1)
        groups.append(v)

    data = bytearray(size)

    def getbit(p):
        b = data[p >> 3]
        return (b >> (p & 7)) & 1

    # Write the 6-bit groups into the bit stream first (need a writable bit view).
    total_bits = size * 8
    for i, group in enumerate(groups):
        start = i * 6
        if start >= total_bits:
            break
        for k in range(6):
            p = start + k
            if p >= total_bits:
                break
            bitval = (group >> k) & 1
            if bitval:
                data[p >> 3] |= (1 << (p & 7))

    print("declared size=%d decoded bytes=%d chars=%d" % (size, len(data), len(groups)))
    return bytes(data)


def parse_binary_xml(data):
    if len(data) < 9:
        print("ERROR: binary too small")
        return None
    magic = int.from_bytes(data[0:4], "little")
    text_len = int.from_bytes(data[4:8], "little")
    print("magic=0x%08X text_len=%d" % (magic, text_len))
    if magic != MAGIC:
        print("ERROR: bad magic")
        return None
    xml_bytes = data[8:8 + text_len]
    try:
        xml_text = xml_bytes.decode("utf-8")
    except UnicodeDecodeError as e:
        print("ERROR: xml decode failed: %s" % e)
        return None
    return xml_text


def main():
    if len(sys.argv) < 2:
        path = r"C:\Users\User\AppData\Roaming\OpenVoxTuner\OpenVoxTuner.settings"
    else:
        path = sys.argv[1]
    val = extract_value(path)
    print("filterState length (chars):", len(val))
    data = decode_bitpacked(val)
    xml_text = parse_binary_xml(data)
    if xml_text is None:
        return
    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError as e:
        print("ERROR: XML parse failed: %s" % e)
        print(xml_text[:2000])
        return

    print("root tag:", root.tag)
    for token in ("harmonyFormant", "harmonyAttack", "voiceType", "harmony_formant"):
        print("contains %-16s in xml_text: %s" % (token, token in xml_text))
    # Show context around each occurrence of the slot-field tokens.
    import re
    for token in ("harmonyFormant", "harmonyAttack", "voiceType"):
        for m in re.finditer(re.escape(token), xml_text):
            s = max(0, m.start() - 60)
            print("  ...%s..." % xml_text[s:m.end() + 40].replace("\n", " "))

    # Enumerate ALL AB_A / AB_B elements (getChildByName reads the FIRST).
    import re as _re
    print("\nroot children tags: ", [c.tag for c in list(root)[:80]])
    allslots = []
    def walk(el):
        if el.tag in ("AB_A", "AB_B"):
            allslots.append(el)
        for c in el:
            walk(c)
    walk(root)
    print("total AB_A/AB_B elements:", len(allslots))
    for i, el in enumerate(allslots):
        print("  [%d] %s type=%s vibrato=%s humanize=%s formant=%s amount=%s correction=%s upward=%s"
              % (i, el.tag, el.get("harmonyType"), el.get("vibratoPreserve"),
                 el.get("humanize"), el.get("harmonyFormant"), el.get("amount"),
                 el.get("correctionMode"), el.get("upwardCompEnable")))
    for slot, name in ((0, "AB_A"), (1, "AB_B")):
        el = root.find(name)
        if el is None:
            print("%s: <missing>" % name)
            continue
        attrs = dict(el.attrib)
        for skip in ("PITCH_CURVE",):
            pass
        print("%s: %s" % (name, attrs))
    # Also dump the root harmony_type param if present.
    valnode = root.find("value") or root
    print("abActiveSlot=%s" % root.get("abActiveSlot", "<missing>"))


if __name__ == "__main__":
    main()
