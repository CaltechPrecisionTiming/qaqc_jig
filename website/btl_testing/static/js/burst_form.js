function press(event) {
  if (event.keyCode == 13 && !event.shiftKey) {

    //Stops enter from creating a new line
    event.preventDefault();
    submitForm();
    return true;
  }

  function submitForm() {
    //submits the form.
    document.burst_form.submit();
  }
}
