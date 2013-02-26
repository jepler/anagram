var request = null, lastquery = '';

function updatejax() {
    console.log('updatejax?');
    if(!request) return;
    console.log('updatejax?' + request.readyState + " ." + request.responseType + ".");
    if(request.readyState > 1) {
        console.log('updatejax ' + request.readyState);
        $('#results').text(request.response);
        if(request.readyState == 4) {
            clearjax();
        }
    }
}

function clearjax() {
    var query = $('#query').val();
    if (query != lastquery) queuejax();
    else request = null;
}

function makejax() {
    try { return new XMLHttpRequest(); } catch(e) {}
    try { return new ActiveXObject("Msxml2.XMLHTTP"); } catch (e) {}
    return null;
}

function queuejax (suffix = ' -50') {
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
    request.send(null);
    window.history.replaceState({'q': query + suffix}, '',
        document.location.toString().replace(/[?#].*$/, "")
        + '?' + $.param({'q': query + suffix}));
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
