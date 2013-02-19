var request = null, lastquery = '';

function updatejax(data) {
console.log('updatejax');
    $('#results').text(data);
}

function clearjax() {
console.log('clearjax');
    var query = $('#query').val();
    if (query != lastquery) queuejax();
    else request = null;
}

function queuejax () {
console.log('queuejax');
    var query = $('#query').val();
    lastquery = query;
    var loc = (
        document.location.toString().replace(/\?.*$/, "")
        + '?' + $.param({'p': 1, 'q': query + ' -50'}));
console.log(loc);
    request = $.ajax({'url': loc, 'cache': true})
        .done(updatejax)
        .always(clearjax);
}

function fulljax (e) {
    if(e) e.preventDefault();
    if(request) request.abort();

console.log('fulljax');
    var query = $('#query').val();
    lastquery = query;
    var loc = (
        document.location.toString().replace(/\?.*$/, "")
        + '?' + $.param({'p': 1, 'q': query}));
console.log(loc);
    request = $.ajax({'url': loc, 'cache': true})
        .done(updatejax)
        .always(clearjax);
console.log('fulljax returning false');
    return false;
}

function mayjax() {
console.log('mayjax');
    if(request) return;
    var query = $('#query').val();
    if(query == '' || query == lastquery) return;
    queuejax();
}

$('#query').keyup(mayjax).change(mayjax);
$('#f').submit(fulljax);
