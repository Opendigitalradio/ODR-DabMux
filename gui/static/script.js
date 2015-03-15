$(document).ready(function() {
    // Only show first tab
    $('#info div:not(:first)').hide();

    // Handle clicks on tabs to change visiblity of panes
    $('#info-nav li').click(function(event) {
        event.preventDefault();
        $('#info div').hide();
        $('#info-nav .current').removeClass("current");
        $(this).addClass('current');
        var clicked = $(this).find('a:first').attr('href');
        $('#info ' + clicked).fadeIn('fast');
    }).eq(0).addClass('current');

});

