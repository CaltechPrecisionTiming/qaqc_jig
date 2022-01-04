// thanks tony ;)
function isNumber(x)
{
    return !isNaN(x) && (x != null);
}

function isPlottable(pair){
    return (isNumber(pair.run) && isNumber(pair.value))
}

cratelabels = ['Average']
for (var i = 0 ; i < 19 ; i++){
    cratelabels.push('Crate ' + i.toString())
}

function mkCrateCheckboxes(divid, pfx, manager){
    // returns function to update the crate mask, and refresh the plot
    // needs to be pre-defined, else crate index is ref'd after loop :(
    function update_mask(i){
        function rv(){
            mask = 2<<i
            if ($(this).is(':checked') && (manager.availmask & mask)){
                manager.plotmask |= mask    // set bit i
            }
            else{
                manager.plotmask &= ~(mask) // clear bit i
            }
            // update
            manager.refresh(manager)
        }
        return rv
    }

    // crates arranged in a grid...
    var nCrates = 20
    nCols = 2
    nRows = Math.floor(nCrates/nCols) + 1
    nCols += 1
    for (var iRow = 0 ; iRow < nRows ; iRow++){
      $('#' + divid).append("<div class=\"row\">")
      for (var iCol = 0 ; iCol < nCols ; iCol++){
        var i = iRow + iCol*nRows
        if (!(i < nCrates)) continue
        var cbid = pfx + 'crate' + i.toString()
        var label = cratelabels[i]
        cbox = '<input id="' + cbid + '" type="checkbox" value="' + i + '"'
        if (manager.plotmask & 2<<i){
            cbox += ' checked'
        }
        cbox += '>'
        label = '<label for="' + i + '">' + label + '</label>'

        // add the checkbox and its label
        $('#' + divid).append(cbox, label)

        // when we check on/off, we toggle the plotting of each crate
        $('#' + cbid).change(update_mask(i))
      }
      $('#' + divid).append('</br>')
    }
}

// set up plot manager
// makes a collection of checkboxes to control which crates are shown
function mkPlotManager(divid, availmask, plotmask){
    // manager just houses the container div, bitmask of which crates to show, 
    // and the function to call to refresh the plot (passed to each checkbox)
    mgr = {
            // FIXME should be better...
            'availmask': availmask,
            'plotmask': plotmask, // a bitmask
            'refreshers': [],
            'refresh': function(m){
                            cblist = m.refreshers
                            for (var i = 0 ; i < m.refreshers.length ; i++){
                                m.refreshers[i]()
                            }
                        }
          }

    mkCrateCheckboxes(divid, divid + '_checkbox_', mgr)

    return mgr
}

function mkCrateLevelPlot(divid, series, manager, 
                          thetitle, thexlabel, theylabel, ymax){
    // first, trim out crap data points
    for (var i = 0 ; i < series.length ; i++)
        series[i].filter(isPlottable)
    //
    // when called, redraws the plot
    manager.refreshers.push(function(){
        var ns = 0
        theseries = []
        labels = []
        for (var i = 0 ; i < 20 ; i++){
            if (manager.plotmask & 2<<i){
                theseries.push(series[i])
                labels.push(cratelabels[i])
                ns += 1
            }
        }
        if (ns < 1)
            charttype = 'missing-data'
        else
            charttype = 'line'

        console.log(theseries)
        MG.data_graphic({
            title: thetitle, 
            data: theseries,
            width: $('#' + divid).width(),
            height: 240, 
            left: 100,
            right: 100,
            target: '#' + divid, 
            transition_on_update: true,
            missing_is_hidden: true,
            x_sort: true,
            x_accessor: 'date', 
            x_label: thexlabel, 
            y_label: theylabel, 
            max_y: ymax,
            area: false,
            interpolate: 'linear',
            chart_type: charttype,
            legend: labels,
            })
    })

    $('#' + divid).append('</br>')
    manager.refresh(manager)
}
