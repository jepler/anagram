var request = null, lastquery = '';

function updatejax(data) {
    $('#results').text(data);
}

function clearjax() {
    var query = $('#query').val();
    if (query != lastquery) queuejax();
    else request = null;
}

function queuejax () {
    var query = $('#query').val();
    lastquery = query;
    var loc = (
        document.location.toString().replace(/\?.*$/, "")
        + '?' + $.param({'p': 1, 'q': query + ' -50'}));
    request = $.ajax({'url': loc, 'cache': true})
        .done(updatejax)
        .always(clearjax);
}

function fulljax (e) {
    if(e) e.preventDefault();
    if(request) request.abort();

    var query = $('#query').val();
    lastquery = query;
    var loc = (
        document.location.toString().replace(/\?.*$/, "")
        + '?' + $.param({'p': 1, 'q': query}));
    request = $.ajax({'url': loc, 'cache': true})
        .done(updatejax)
        .always(clearjax);
    return false;
}

function mayjax() {
    if(request) return;
    var query = $('#query').val();
    if(query == '' || query == lastquery) return;
    queuejax();
}

$('#query').keyup(mayjax).change(mayjax);
$('#f').submit(fulljax);
