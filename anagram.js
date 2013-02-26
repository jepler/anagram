var request = null, lastquery = '';

function updatejax() {
    if(!request) return;
    if(request.readyState > 1) {
        if(request.response)
            $('#results').text(request.response);
        else if(request.responseText)
            $('#results').text(request.responseText);
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
    var loc = (
        document.location.toString().replace(/[?#].*$/, "")
        + '?' + $.param({'p': 1, 'q': query + suffix}));
    request = makejax();
    request.open("GET", loc, true);
    request.overrideMimeType('text/plain')
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
    if(e) e.preventDefault();
    if(request) request.abort();
    queuejax('');
    return false;
}

function mayjax() {
    if(request) return;
    var query = $('#query').val();
    if(query == '' || query == lastquery) return;
    queuejax(' -50');
}

$('#query').keyup(mayjax).change(mayjax);
$('#f').submit(fulljax);
