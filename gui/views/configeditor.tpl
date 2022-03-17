<!DOCTYPE html>
<html>
  <head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ODR-DabMux Configuration Editor</title>
    <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css"> 
  </head>
<body class="w3-container">
  <h1>Configuration for {{version}}</h1>

  <p><a href="/config">Reload</a></p>

  {% if message %}
    <p>{{message}}</p>
  {% endif %}

  <form action="/config" method="post">
      <p>
      <textarea name="config" cols="60" rows="30">{{config}}</textarea>
      </p>

      <p>
        <input type="submit" value="Update ODR-DabMux configuration"></input>
      </p>
    </form>

  </body>
</html>