<!DOCTYPE html>
<html>
  <head>
    <title>ODR-DabMux Configuration</title>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css">
  </head>

  <body class="w3-container">
    <h1 class="w3-blue-grey">Remote-Control: module {{module}}</h1>
    <div class="w3-card-4"> 
      <form class="w3-container" method="post">
        <p />
        {% if not list %}
          <label>{{param}}:</label>
          <input name="newvalue" type="text" value="{{value.decode()}}" autofocus>
        {% else %}
          <label>{{label}}:</label>
          <select id="newvalue" name="newvalue">
            {% for l in list %}
              {% if (l["value"] == value.decode()) %}
                <option selected value={{l["value"]}}>{{l["desc"]}}</option>
              {% else %}
                <option value={{l["value"]}}>{{l["desc"]}}</option>
              {% endif %}
            {% endfor %}
          </select>
        {% endif %}
        <p />
        <button class="w3-button w3-blue-grey" type="submit">Update</button>
        <p />
      </form>
    </div>
  </body>

</html>