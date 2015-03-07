<!DOCTYPE html>
<html><head>
<title>ODR-DabMux Configuration Dump</title>
<style>
body {
    font-family: sans;
}
</style>
</head>
<body>
    <h1>Configuration for {{version}}</h1>
    <h2>Services</h2>
    <ul>
        % for s in services:
            <li>{{s.name}}: <i>{{s.label}} ({{s.shortlabel}})</i> &mdash; id = {{s.srvid}}</li>
        % end
    </ul>
    <h2>Subchannels</h2>
    <ul>
        % for s in subchannels:
            <li>{{s.name}}: <i>{{s.type}}</i> &mdash; {{s.inputfile}}; {{s.bitrate}}kbps</li>
        % end
    </ul>
    <h2>Components</h2>
    <ul>
        % for s in components:
            <li>{{s.name}}: <i>{{s.label}} ({{s.shortlabel}})</i> &mdash; service {{s.service}}; subchannel {{s.subchannel}}; figtype {{s.figtype}}</li>
        % end
    </ul>
</body>
</html>

