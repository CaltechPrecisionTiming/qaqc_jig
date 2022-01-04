function update_files(name, interval) {
    // update the list of files in the l2 state `name`

    $.getJSON($SCRIPT_ROOT + '/get_l3?name=' + name).done(function(obj) {
        $('#' + name + ' tbody tr').remove();
        for (var i=0; i < obj.files.length; i++) {
            var mom = moment.tz(Number(obj.times[i])*1000, "America/Toronto");
            var tr = $('<tr>')
                .append($('<td>').text(obj.files[i]))
                .append($('<td>').text(mom.fromNow()));
            $('#' + name).find('tbody').append(tr);
        }
        setTimeout(function() {update_files(name, interval); }, interval*1000);
    });
}

function get_SH_settings(interval) {
  // update the current stonehenge settings loaded from redis

  $.getJSON($SCRIPT_ROOT + '/get_SH').done(function(obj) {
    var high_td = document.getElementById('high').innerHTML = obj.settings[8];
    var highEvs_td = document.getElementById('highEvs').innerHTML = obj.settings[9];
    var highSurv_td = document.getElementById('highSurv').innerHTML = obj.settings[10];
    var nhit3_td = document.getElementById('3Evt').innerHTML = obj.settings[0];
    var nhit5_td = document.getElementById('5Evt').innerHTML = obj.settings[1];
    var nhit7_td = document.getElementById('7Evt').innerHTML = obj.settings[2];
    var nhit10_td = document.getElementById('10Evt').innerHTML = obj.settings[3];
    var window_td = document.getElementById('window').innerHTML = obj.settings[4];
    var xwindow_td = document.getElementById('pre').innerHTML = obj.settings[5];
    var ywindow_td = document.getElementById('post').innerHTML = obj.settings[6];
    var ext_td = document.getElementById('ext').innerHTML = obj.settings[7];
    setTimeout(function() {get_SH_settings(interval); }, interval*1000);
  });
}
