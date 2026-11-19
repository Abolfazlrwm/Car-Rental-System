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
Module['printErr'] = function (t) { __out.push('[err] ' + t); };

// login -> My reservations (blank answers the series prompt) ->
// logout -> exit. Queued up front, newline per line exactly as a
// keyboard would deliver them.
['2', 'ali', 'customer123', '3', '\n', '12', '3'].forEach(
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
            ['login', text.indexOf('Welcome, ali (Customer)!') !== -1],
            ['reservations', text.indexOf('no reservations yet') !== -1],
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
