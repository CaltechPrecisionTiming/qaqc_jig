function update_search() {
    var selected = document.getElementById("search-by");
    var start = document.getElementById("start");
    var end = document.getElementById("end");
    var value = selected.value;
    if (value == "date"){
      start.type = "date";
      end.type = "date";
    } else {
      if (start.type = "date"){start.type = "number";end.type = "number";}
    }
}

function validate_form() {
  var start = document.getElementById("start").value;
  var end = document.getElementById("end").value;
  var selected = document.getElementById("search-by").value;
  if ( (start == "") || (end == "") ){
    alert("Please fill both Start and End fields !");
    return false;
  }
  else if ( (selected == "date") && (end < start) ){
    alert("End value must be higher than Start value !");
    return false;
  } else {
    if ( parseInt(end) < parseInt(start) ){
    alert("End value must be higher than Start value !");
    return false;
    }
  }
}

function fill_boxes() {
  const queryString = window.location.search;
  const urlParams = new URLSearchParams(queryString);
  const search = urlParams.get('search');
  if (search){
    var selected = document.getElementById("search-by");
    var value = selected.value;
    var start = document.getElementById("start");
    var end = document.getElementById("end");
    if (search == "run"){i=0;}
    else if (search == "gtid"){i=1;}
    else if (search == "date"){i=2; start.type = "date"; end.type = "date";}
    selected.selectedIndex = i;
    start.value = urlParams.get('start');
    end.value = urlParams.get('end');
  }
}
