var updatefunc = function(event) {
    $('#statdata p').remove();
    $.getJSON('/stats.json', function(result) {
        $.each(result, function(name) {
            // TODO: use a hidden template inside the DOM instead
            // of building the HTML here
            $("<p></p>")
                .append(result[name]['inputstat']['num_underruns'])
                .appendTo('#statdata');
        });
    });
}

// Handle clicks on the to change visiblity of panes
setInterval(updatefunc, 1000);


