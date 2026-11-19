/* Keyboard bridge between the demo page and the compiled program.
 *
 * The C++ side (readLine in mainApp.cpp, browser build) waits for a
 * FULL line: it polls __crsLineReady() and sleeps between polls with
 * emscripten_sleep (Asyncify), then __crsTakeLine() copies the bytes
 * in one go. The page feeds every keystroke through Module.pushInput;
 * bytes accumulate until Enter (\n) makes a line ready, exactly the
 * way a keyboard and a terminal would deliver them.
 *
 * Exposed to the page:
 *   Module.pushInput(text) — queue text as if typed
 *   Module.inputPending()  — bytes still unread
 *
 * The demo page never signals EOF, so the program can only exit
 * through its own Exit menu — reloading the page reseeds a fresh
 * demo fleet.
 */
var __crsInput = [];

Module['pushInput'] = function (text)
{
    var bytes = new TextEncoder().encode(
        String(text).replace(/\r\n/g, '\n').replace(/\r/g, '\n'));
    for (var i = 0; i < bytes.length; i++)
        __crsInput.push(bytes[i]);
};

Module['inputPending'] = function () { return __crsInput.length; };

/* Called from the compiled program (EM_JS helpers in mainApp.cpp). */
function __crsLineReady()
{
    return __crsInput.indexOf(10) !== -1; // 10 == '\n'
}

function __crsTakeLine(buf, maxlen)
{
    var idx = __crsInput.indexOf(10);
    if (idx === -1)
        return 0;
    var n = Math.min(idx, maxlen);
    for (var i = 0; i < n; i++)
        Module.HEAPU8[buf + i] = __crsInput[i];
    __crsInput.splice(0, idx + 1); // consumed through the newline
    return n;
}

/* Byte-exact terminal output: route every byte the program writes to
 * the page as it is written (prompts included), decoding UTF-8 with
 * the streaming decoder so multi-byte characters survive byte-sized
 * writes. Without this the TTY line-batches output and prompts would
 * only surface on the next newline. */
Module['preRun'] = [function ()
{
    TTY.default_tty_ops.put_char = function (tty, val)
    {
        if (val === null || val === undefined || val === 0)
            return;
        if (Module.termByte)
            Module.termByte(val);
    };
}];
