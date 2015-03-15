<!DOCTYPE html>
<html><head>
<title>ODR-DabMux Services</title>
<link rel="stylesheet" href="static/style.css" />
</head>
<body>
    <h1>Services for {{version}}</h1>
    <table>
    <tr>
        <th>name</th>
        <th>id</th>
        <th>label</th>
        <th>pty</th>
        <th>language</th>
    </tr>
        % for s in services:
            <tr><td>{{s.name}}</td>
                <td>{{s.id}}</td>
                <td>{{s.label}} ({{s.shortlabel}})</td>
                <td>{{s.pty}}</td>
                <td>{{s.language}}</td>
            </tr>
        % end
    </ul>
</body>
</html>

