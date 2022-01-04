$("#step-menu").on("change", function() {
    window.location.replace($SCRIPT_ROOT + "/remote-network-monitor?step=" + this.value + "&height=" + height);
});

var context = create_context('#main', step);

function metric(name) {
    var display = name;

    return context.metric(function(start, stop, step, callback) {
        d3.json($SCRIPT_ROOT + '/metric' + 
                '?expr=' + name +
                '&start=' + start.toISOString() +
                '&stop=' + stop.toISOString() +
                '&now=' + new Date().toISOString() +
                '&step=' + Math.floor(step/1000), function(data) {
                if (!data) return callback(new Error('unable to load data'));
                return callback(null,data.values);
        });
    }, display);
}

function add_horizon(expressions, format, colors, extent) {
    var horizon = context.horizon().height(Number(height));

    if (typeof format != "undefined") horizon = horizon.format(format);
    if (typeof colors != "undefined" && colors) horizon = horizon.colors(colors);
    if (typeof extent != "undefined") horizon = horizon.extent(extent);

    d3.select('#main').selectAll('.horizon')
        .data(expressions.map(metric), String)
      .enter().insert('div','.bottom')
        .attr('class', 'horizon')
        .call(horizon)
        .on('click', function(d, i) {
            var domain = context.scale.domain();
            var params = {
                name: expressions[i],
                start: domain[0].toISOString(),
                stop: domain[domain.length-1].toISOString(),
                step: Math.floor(context.step()/1000)
            };
            window.open($SCRIPT_ROOT + "/graph?" + $.param(params), '_self');
        });
}

function add_remote_control_room(name) {
    add_horizon([name + "-heartbeat"],format_int,null,[0,4]);
    add_horizon([name + "-packets-sent"],format_int);
    add_horizon([name + "-packets-lost"],format_int);

    var horizon = context.horizon()
        .height(Number(url_params.height))
        .format(my_percentage_format)
        .extent([0,0.05]);

    d3.select('#main').selectAll('.horizon')
        .data([(metric(name + '-packets-lost').divide(metric(name + '-packets-sent')))],String)
      .enter().insert('div','.bottom')
        .attr('class', 'horizon')
        .call(horizon);
}

add_remote_control_room("crug");
add_remote_control_room("crag");
add_remote_control_room("cruc");
add_remote_control_room("crlu");
add_remote_control_room("crox");
add_remote_control_room("crup");
add_remote_control_room("crab");
add_remote_control_room("crlip");
add_remote_control_room("crum");

context.on("focus", function(i) {
  d3.selectAll(".value").style("right", i === null ? null : context.size() - i + "px");
});
