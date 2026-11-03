// Closure variable lookups after a sloppy eval has added a var somewhere.
var failures = 0;
function check(name, got, want) {
    if (got !== want) {
        failures++;
        print("FAIL " + name + ": got " + got + ", want " + want);
    }
}

function makeBench() {
    var a = 1, b = 2, c = 3;
    function middle() {
        var m = 4;
        eval(""); // declares nothing, so the watchpoint stays intact
        return function inner(n) {
            var s = 0;
            for (var i = 0; i < n; i++)
                s += a + b + c + m;
            return s;
        };
    }
    return middle();
}

function bench(label) {
    var f = makeBench();
    var best = 1e9;
    for (var round = 0; round < 3; round++) {
        var start = Date.now();
        var s = f(20000);
        var ms = Date.now() - start;
        if (ms < best)
            best = ms;
        check(label + " sum", s, 200000);
    }
    print("BENCH " + label + " " + best + " ms");
}

bench("before-injection");

// Shadowing var added to a scope in between.
function t1() {
    var v = "outer";
    function mid() {
        eval("var v = 'mid'");
        return function () { return v; };
    }
    return mid()();
}
check("t1", t1(), "mid");

// Closure created and used before the eval shadows its variable.
function t2() {
    var v = "outer";
    function mid() {
        var g = function () { return v; };
        var before = g();
        eval("var v = 'mid'");
        return before + "," + g();
    }
    return mid();
}
check("t2", t2(), "outer,mid");

// Another name added to the scope in between.
function t3() {
    var v = "outer";
    function mid() {
        eval("var w = 'w'");
        return function () { return v + w; };
    }
    return mid()();
}
check("t3", t3(), "outerw");

// The added var is deleted again.
function t4() {
    var v = "outer";
    function mid() {
        eval("var v = 'mid'");
        var g = function () { return v; };
        var a = g();
        delete v;
        return a + "," + g();
    }
    return mid();
}
check("t4", t4(), "mid,outer");

// Writes through the closure.
function t5() {
    var v = 1;
    function mid() {
        eval("var x = 0");
        var inc = function () { v = v + 1; return v; };
        inc();
        inc();
        return inc();
    }
    var r = mid();
    return r + ":" + v;
}
check("t5", t5(), "4:4");

// Deeper chain, var added halfway.
function t6() {
    var v = "top";
    function a() {
        var keepA = 1;
        function b() {
            eval("var v = 'b'");
            function c() {
                var keepC = 2;
                return function () { return v + keepA + keepC; };
            }
            return c()();
        }
        return b();
    }
    return a();
}
check("t6", t6(), "b12");

// Block scope in between.
function t7() {
    var v = "outer";
    function mid() {
        eval("var z = 1");
        var f;
        {
            let blockVar = 2;
            f = function () { return v + blockVar; };
        }
        return f();
    }
    return mid();
}
check("t7", t7(), "outer2");

// Catch scope in between.
function t8() {
    var v = "outer";
    function mid() {
        eval("var y = 1");
        var f;
        try {
            throw "e";
        } catch (err) {
            f = function () { return v + err; };
        }
        return f();
    }
    return mid();
}
check("t8", t8(), "outere");

// with stays dynamic.
function t9() {
    var v = "outer";
    var o = { v: "with" };
    with (o) {
        return (function () { return v; })();
    }
}
check("t9", t9(), "with");

// Same closure, many activations, only some of them shadowed.
function t10() {
    var v = "outer";
    var out = [];
    function mid(shadow) {
        if (shadow)
            eval("var v = 'mid'");
        return function () { return v; };
    }
    for (var i = 0; i < 6; i++)
        out.push(mid(i % 2)());
    return out.join(",");
}
check("t10", t10(), "outer,mid,outer,mid,outer,mid");

bench("after-injection");
print("Scope injection test: " + failures + " failures");
