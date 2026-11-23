/* Node smoke test for the WebAssembly build (not shipped).
 *
 * Same keyboard bridge as docs-site/demo/pre.js, but the scripted
 * session is queued up front (lines complete immediately, so the
 * program never really waits) and stdout is captured for assertions.
 * The session: login as ali, glance at My reservations, logout, exit.
 */
var __crsInput = [];

Module['pushInput'] = function (text)
{
    var bytes = new TextEncoder().encode(text);
    for (var i = 0; i < bytes.length; i++)
        __crsInput.push(bytes[i]);
};

Module['inputPending'] = function () { return __crsInput.length; };

function __crsLineReady()
{
    return __crsInput.indexOf(10) !== -1;
}

function __crsTakeLine(buf, maxlen)
{
    var idx = __crsInput.indexOf(10);
    if (idx === -1)
        return 0;
    var n = Math.min(idx, maxlen);
    for (var i = 0; i < n; i++)
        Module.HEAPU8[buf + i] = __crsInput[i];
    __crsInput.splice(0, idx + 1);
    return n;
}

var __out = [];
Module['print'] = function (t) { __out.push(t); };

// (v3.10.0) The scripted session saves the consolidated PDF (manager
// option 21); every file the program hands over lands here.
var __offers = [];
Module['crsFileOffered'] = function (name, bytes)
{
    __offers.push({ name: name, size: bytes.length, head: bytes.length ? bytes[0] : -1 });
};
Module['printErr'] = function (t) { __out.push('[err] ' + t); };

// login as manager -> save all three reports as one PDF (option 21,
// November 2026) -> save & logout -> exit. Queued up front, newline
// per line exactly as a keyboard would deliver them.
['2', 'admin', 'manager123', '21', '2026', '11', '15', '3'].forEach(
    function (line) { Module['pushInput'](line + '\n'); });

Module['onRuntimeInitialized'] = function ()
{
    // The program polls for full lines and sleeps between polls
    // (emscripten_sleep unwinds callMain), so poll alongside it:
    // assert once the scripted session is consumed or a deadline
    // passes.
    Module.callMain([]);
    var deadline = Date.now() + 20000;
    var poll = setInterval(function ()
    {
        var pending = Module['inputPending']();
        if (pending !== 0 && Date.now() < deadline)
            return;
        clearInterval(poll);
        var text = __out.join('\n');
        var checks = [
            ['banner', text.indexOf('Console edition') !== -1],
            ['seeded fleet', text.indexOf('First run: demo fleet') !== -1],
            ['login', text.indexOf('Welcome, admin (Manager)!') !== -1],
            ['pdf saved', text.indexOf('Saved to reports/all_reports_2026-11.pdf') !== -1],
            ['pdf offered once', __offers.length === 1],
            ['pdf bytes',
             __offers.length === 1 && __offers[0].name === 'all_reports_2026-11.pdf' &&
                 __offers[0].head === 37 && __offers[0].size > 800000],
            ['clean exit', text.indexOf('Goodbye!') !== -1],
            ['no stderr noise', !text.includes('[err]')],
        ];
        var failed = 0;
        checks.forEach(function (c)
        {
            console.log((c[1] ? 'PASS ' : 'FAIL ') + c[0]);
            if (!c[1])
                failed++;
        });
        console.log(failed === 0 ? 'WASM SMOKE TEST GREEN' : 'WASM SMOKE TEST RED');
        process.exit(failed === 0 ? 0 : 1);
    }, 100);
};
