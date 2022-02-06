<!DOCTYPE html>
<html>
  <head>
    <title>ODR-DabMux Services</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css"> 
  </head>
  <body class="w3-container">
    <h1>Services for {{version}}</h1>
    <table class="w3-table w3-striped w3-bordered">
      <tr class="w3-blue-grey">
        <th>Service</th>
        <th>Id</th>
        <th>Label</th>
        <th>Short label</th>
        <th>Program type</th>
        <th>Language</th>
      </tr>
      {% for s in services %}
        <tr>
          <td>{{s.name}}</td>
          <td>{{s.id}}</td>
          <td>{{s.label}}</td>
          <td>{{s.shortlabel}}</td>
          <td>{{s.pty}}</td>
          <td>{{s.language}}</td>
        </tr>
      {% endfor %}
    </table>
  </body>
</html>