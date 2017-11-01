var timerId = -1;

$(function() {
    // poll for the current temperature every poll interval ms..
    setTimeout(function() {
        $.get('/cur_temp', {}, function(temp) {
            $('#current-temp').data('cval', temp).html(temp);
        });
    }, ($('#poll-interval').val() * 1000));
    
    // poll for the heat status every 5 seconds..
    setTimeout(function() {
        $.get('/heat_status', {}, function(status) {
            $('#heat-status').data('cval', status).html("Furnace is currently " + status);
            if (status == "ON") {
                $('body').addClass('heat-is-on');
            } else {
                if ($('body').hasClass('heat-is-on')) {
                    $('body').removeClass('heat-is-on');
                }
            }
        });
    }, 5000);
    
    $('#target-up').click(function(e) {
        var new_val = get_target() + 1;
        if (new_val >= 80) {
            alert("I don't care how much you like sitting on the vent, you are not setting the thermostat over 79.");
        } else {
            update_target(new_val);
        }
        e.preventDefault();
    });
    
    $('#target-down').click(function(e) {
        var new_val = get_target() - 1;
        if (new_val < 60) {
            alert("Target temperature needs to be 60 or higher... you want your pipes to freeze?!");
        } else {
            update_target(new_val);
        }
        e.preventDefault();
    });
    
    $('#poll-interval').change(function(e) {
        var new_val = this.val();
        if (new_val < )
        update_pi(this.val());
    });
    
});

function get_target () {
    return $.trim($('#target-temp').text());
}

function update_pi (new_val) {
    $.post('/update', {
        "sample_rate": new_val
    });
}

function update_target (new_val) {
    if (timerId != -1) {
        clearTimeout(timerId);
    }
    
    $('#target-temp').html(new_val);
    $('#table-target-temp').html(new_val);
    timerId = setTimeout(function() {
        $.post('/update', {
            "temp_target": new_val
        },  
        function(e) {
            $('#target-temp').fadeIn(50).fadeOut(75).fadeIn(75).fadeOut(100).fadeIn(100);
        });
    }, 500);
}