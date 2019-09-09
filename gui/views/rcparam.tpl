<!DOCTYPE html>
<html><head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <title>ODR-DabMux Configuration</title>
    <link rel="stylesheet" href="/static/style.css" type="text/css" media="screen" charset="utf-8"/>
    <script type="text/javascript" src="/static/jquery-1.10.2.min.js"></script>
    <script type="text/javascript" src="/static/intercooler-1.0.1.min.js"></script>
</head>
<body>
  <h1>Remote-Control of module {{module}}</h1>

  <form ic-on-error="alert(str)" ic-post-to="/rc/{{module}}/{{param}}">
    <div class="form-group">
      <label>Parameter <i>{{param}}</i> value: </label>
      <input class="form-control" name="newvalue" type="text" value="{{value}}">
    </div>
    <button class="btn btn-default">Update value</button>
  </form>

</body>
</html>

