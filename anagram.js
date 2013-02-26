var request = null, lastquery = '';

function updatejax() {
    if(!request) return;
    if(request.readyState > 1) {
        var response = '';
        try {
            if(request.response)
                response = request.response;
            else if(request.responseText)
                response = request.responseText;
        } catch(unused) {}
        if(response != '') {
            response = response.replace(/&/g, "&amp;");
            response = response.replace(/</g, "&lt;");
            response = response.replace(/>/g, "&gt;");
            var el = document.getElementById('results');
            if(el.outerHTML) {
                el.outerHTML = '<pre id="results">' + response + '</pre>';
            } else {
                el.innerHTML = response;
            }
        }
        if(request.readyState == 4) {
            clearjax();
        }
    }
}

function clearjax() {
    request = null;
    mayjax();
}

function makejax() {
    try { return new XMLHttpRequest(); } catch(e) {}
    try { return new ActiveXObject("Msxml2.XMLHTTP"); } catch (e) {}
    return null;
}

function queuejax (suffix) {
    var query = $('#query').val();
    lastquery = query;
    query = query.replace(/[=<>-]\s*$/, '')
    var loc = (
        document.location.toString().replace(/[?#].*$/, "")
        + '?' + $.param({'p': 1, 'q': query + suffix}));
    request = makejax();
    request.open("GET", loc, true);
    try {
        request.overrideMimeType('text/plain')
    } catch (unused) {}
    request.onreadystatechange = updatejax;
    request.onprogress = updatejax;
    request.onloadend = updatejax;
    request.onload = updatejax;
    request.onerror = updatejax;
    request.send(null);
    if(window && window.history && window.history.replaceState)
        window.history.replaceState({}, '',
            document.location.toString().replace(/[?#].*$/, "")
            + '?' + $.param({'q': query}));
}

function fulljax (e) {
    if(request) request.abort();
    queuejax('');
    if(e) e.preventDefault();
    return false;
}

function mayjax() {
    if(request) return;
    var query = $('#query').val();
    if(query == '' || query == lastquery) return;
    queuejax(' -50');
}

lastquery = $('#query').val();
$('#query').keyup(mayjax).change(mayjax);
$('#f').submit(fulljax);
