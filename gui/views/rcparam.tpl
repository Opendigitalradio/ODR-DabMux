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

  <body class="w3-container">
    <h1 class="w3-blue-grey">Remote-Control: module {{module}}</h1>
    <div class="w3-card-4"> 
      <form class="w3-container" ic-on-error="alert(str)" ic-post-to="/rc/{{module}}/{{param}}">
        <p />
        % if (len(list) == 0):
          <label>{{param}}:</label>
          <input name="newvalue" type="text" value="{{value}}" autofocus>
        % else:
          <label>{{label}}:</label>
          <select id="newvalue" name="newvalue">
            % for l in list:
              % if (bytes(l["value"], 'utf-8') == value):
                <option selected value={{l["value"]}}>{{l["desc"]}}</option>
              % else:
                <option value={{l["value"]}}>{{l["desc"]}}</option>
              % end
            % end
          </select>
        % end
        <p />
        <button class="w3-button w3-blue-grey">Update</button>
        <p />
      </form>
    </div>
  </body>

</html>