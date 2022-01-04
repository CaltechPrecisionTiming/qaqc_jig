si_format = d3.format('.3s');

function my_si_format(d) {
    if (!$.isNumeric(d))
        return '-';
    else
        return si_format(d);
}

/* Draw the scatter plot with the default paramters */
function draw_scatter_plot_disc(data, xmax, name, xname, ymin, ymax){

    if( data != undefined ){

        var margin = {top: 80, right: 25, bottom: 80, left: 90},
            width = $(name).width() - margin.left - margin.right,
            height = 400;

        var x = d3.scale.linear()
            .domain([0, xmax])
            .range([ 0, width ]);

        var y = d3.scale.linear()
                 .domain([ymin-0.5, ymax+0.5])
                 .range([height, 0]);

        var chart = d3.select(name)
            .append('svg:svg')
            .attr('width', width + margin.right + margin.left)
            .attr('height', height + margin.top + margin.bottom)
            .attr('class', 'chart');

        var main = chart.append('g')
            .attr('transform', 'translate(' + margin.left + ',' + margin.top + ')')
            .attr('width', width)
            .attr('height', height)
            .attr('class', 'main');

        var div = d3.select("body").append("div")
            .attr("class", "tooltip")
            .style("opacity", 0);

        var xAxis = d3.svg.axis()
            .scale(x)
            .orient('bottom')
            .tickSize(4)
            .ticks(4);

        main.append('g')
            .attr('transform', 'translate(0,' + height + ')')
            .attr('class', 'main axis date')
            .call(xAxis)
            .selectAll("text")
              .attr("x", 6)
              .attr("dy", "1.2em");

        var yAxis = d3.svg.axis()
            .scale(y)
            .orient('left')
            .tickSize(6)
            .ticks(8, "s");

        main.append('g')
            .attr('transform', 'translate(0,0)')
            .attr('class', 'main axis')
            .call(yAxis);

        d3.selectAll(".tick > text")
            .style("font-size", "12px");

        var g = main.append("svg:g");

        g.selectAll("scatter-dots")
            .data(data)
            .enter().append("svg:circle")
                .attr("cx", function (d,i) { return x(d[0]); } )
                .attr("cy", function (d) { return y(d[1]); } )
                .attr("r", 8)
                .on("mouseover", function(d) {      
                    div.transition()        
                        .duration(200) 
                        .style("opacity", .9);      
                    div.html("(" + (d[0]) + ","  + my_si_format(d[1]) + ")")  
                        .style("left", (d3.event.pageX) + "px")
                        .style("top", (d3.event.pageY - 50) + "px")
                        .style("font-size", "20px");
                    })                  
                .on("mouseout", function(d) {
                    div.transition()
                        .duration(500)
                        .style("opacity", 0);   
                });

        chart.append("g")
            .attr("class", "x axis")
            .attr("transform", "translate(0," + height + ")")
          .append("text")
            .attr("class", "label")
            .attr("x", width-100 )
            .attr("y", 130)
            .style("text-anchor", "end")
            .text(xname)
            .style("font-size", "12px");

        chart.append("g")
            .attr("class", "y axis")
          .append("text")
            .attr("class", "label")
            .attr("transform", "rotate(-90)")
            .attr("dy", "3.0em")
            .attr("x", -160 )
            .style("text-anchor", "end")
            .text("Threshold (DAC Counts)")
            .style("font-size", "12px");
    }
}

