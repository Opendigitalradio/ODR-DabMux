<!DOCTYPE html>
<html>
  <head>
    <title>ODR-DabMux Configuration</title>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css">
    <script type="text/javascript" src="/static/jquery-1.10.2.min.js"></script>
    <script type="text/javascript" src="/static/intercooler-1.0.1.min.js"></script>
  </head>

  <body>
    <h1 class="w3-container w3-blue-grey">Remote-Control of module {{module}}</h1>
    <div class="w3-container">
      <form class="w3-container w3-card-4" ic-on-error="alert(str)" ic-post-to="/rc/{{module}}/{{param}}">
        <p />
        <label>Parameter <b>{{param}}</b></label>
        <input name="newvalue" type="text" value="{{value}}">
        <p />
        <button class="w3-button w3-blue-grey">Update</button>
      </form>
    </div>
  </body>

</html>
