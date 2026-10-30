// Run with the native C-loop shell. Throw on the first failed invariant.
let checks = 0;
function check(value, message) {
    if (!value)
        throw new Error(message);
    ++checks;
}

print("Checking native JavaScript values and arithmetic");
check(0x7fffffff + 1 === 2147483648, "signed boundary");
check((0xffffffff >>> 0) === 4294967295, "unsigned boundary");
check((0x80000000 >>> 0) === 2147483648, "unsigned sign-bit boundary");
check((-1 >>> 0) === 4294967295, "negative to unsigned conversion");
check(Math.fround(2147483647) === 2147483648, "float rounding across signed boundary");
check(new Float32Array([4294967295])[0] === 4294967296, "float rounding across unsigned boundary");
check(Math.fround(1.000000059604644775390625) === 1, "float tie rounds down to even");
check(Math.fround(1.000000178813934326171875) === 1.0000002384185791015625, "float tie rounds up to even");
check(1 / -0 === -Infinity, "negative zero");
// Read operands through mutable storage so these exercise runtime arithmetic.
const numeric = [Number.MAX_VALUE, 2, 0.5, Infinity, NaN, 0];
check(numeric[0] * numeric[1] === Infinity, "double multiply overflow");
check(numeric[0] + numeric[0] === Infinity, "double add overflow");
check(numeric[0] / numeric[2] === Infinity, "double divide overflow");
check(numeric[3] + numeric[1] === Infinity && numeric[3] * numeric[1] === Infinity
    && numeric[3] / numeric[1] === Infinity, "infinity propagation");
check(Number.isNaN(numeric[3] - numeric[3]) && Number.isNaN(numeric[3] / numeric[3])
    && Number.isNaN(numeric[3] * numeric[5]), "invalid arithmetic produces NaN");
check(Number.isNaN(numeric[4] + numeric[1]) && Number.isNaN(numeric[4] * numeric[1])
    && Number.isNaN(numeric[4] / numeric[1]), "NaN propagation");
check(!(numeric[4] < numeric[1]) && !(numeric[4] <= numeric[1])
    && !(numeric[4] > numeric[1]) && !(numeric[4] >= numeric[1])
    && numeric[4] !== numeric[4], "unordered comparison predicates");
check(Math.hypot(numeric[0], numeric[0]) === Infinity, "hypot overflow");
check(Number.MIN_VALUE / numeric[1] === 0
    && (Number.MIN_VALUE * 3) / numeric[1] === Number.MIN_VALUE * 2,
    "gradual underflow rounding");
check(Math.abs(Math.log2(8) - 3) < 1e-12, "C99 math");
check((12345678901234567890n * 9n).toString() === "111111110111111111010", "BigInt");
const view = new DataView(new ArrayBuffer(8));
view.setUint32(0, 0x12345678, true);
check(view.getUint32(0, false) === 0x78563412, "typed-array byte order");
check([7, 1, 4].sort((a, b) => a - b).join(",") === "1,4,7", "array sorting");

print("Checking Unicode, regular expressions and ICU");
check("e\u0301".normalize("NFC") === "\u00e9", "normalization");
check("\ud83d\ude00".codePointAt(0) === 0x1f600, "surrogate pair");
check(/(?<letter>\p{Script=Greek}+)/u.exec("a\u03b1\u03b2").groups.letter === "\u03b1\u03b2", "Unicode regexp");
check("Odyssey".startsWith("O", -1) && "Odyssey".endsWith("y"), "string index conversion");
check(new Intl.NumberFormat("en-US").format(1234.5) === "1,234.5", "number formatting");
check(new Intl.Collator("de").compare("a", "b") < 0, "collation");

print("Checking live objects across repeated garbage collections");
check(typeof gc === "function", "shell GC entry point");
const retained = [];
for (let round = 0; round < 32; ++round) {
    for (let i = 0; i < 256; ++i) {
        const payload = { number: round * 256 + i, text: "payload:" + i };
        const closure = () => payload.number + payload.text.length;
        if (!(i % 64))
            retained.push([payload, closure]);
    }
    gc();
    for (const [payload, closure] of retained)
        check(closure() === payload.number + payload.text.length, "retained closure after GC");
}
check(retained.length === 128, "retained object count");
print("Odyssey JSC test: 0 failures (" + checks + " checks)");
