<!DOCTYPE html>
<html><head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <title>ODR-DabMux Configuration Editor</title>
	<link rel="stylesheet" href="static/style.css" type="text/css" media="screen" charset="utf-8"/>
    <script type="text/javascript" src="static/jquery-1.10.2.min.js"></script>
</head>
<body>
    <h1>Configuration for {{version}}</h1>

    <p><a href="/config">Reload</a></p>

    % if message:
    <p>{{message}}</p>
    % end

    <form action="/config" method="post">
        <p>
        <textarea name="config" cols="60" rows="50">{{config}}</textarea>
        </p>

        <p>
        <input type="submit" value="Update ODR-DabMux configuration"></input>
        </p>
    </form>

</body>
</html>

