<!DOCTYPE html>
<html>
  <head>
    <title>ODR-DabMux Configuration</title>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css">
    <script type="text/javascript" src="static/jquery-1.10.2.min.js"></script>
    <script type="text/javascript" src="static/script.js"></script>
  </head>
  <body>
    <div class="w3-top w3-bar w3-blue-grey">
      <a href="#general" class="w3-bar-item w3-button w3-mobile">General Options</a>
      <a href="#servicelist" class="w3-bar-item w3-button w3-mobile">Services</a>
      <a href="#subchannels" class="w3-bar-item w3-button w3-mobile">Subchannels</a>
      <a href="#components" class="w3-bar-item w3-button w3-mobile">Components</a>
      <a href="#rcmodules" class="w3-bar-item w3-button w3-mobile">RC Modules</a>
    </div>
    <br /><br />
    <div class="w3-container">
      <ul id="general" class="w3-card-4 w3-ul">
        <li class="w3-blue-grey"><h3>General Multiplex Options</h3></li>
        <li>Number of frames to encode: {{g.nbframes}}</li>
        <li>Statistics server port: {{g.statsserverport}}</li>
        <li>Write SCCA field: {{g.writescca}}</li>
        <li>Write TIST timestamp: {{g.tist}}</li>
        <li>DAB Mode: {{g.dabmode}}</li>
        <li>Log to syslog: {{g.syslog}}</li>
      </ul>
      <p />
      <table id="servicelist" class="w3-card-4 w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>Service</th>
          <th>Id</th>
          <th>Label</th>
          <th>Short label</th>
        </tr>
        % for s in services:
          <tr>
            <td>{{s.name}}</td>
            <td>{{s.id}}</td>
            <td>{{s.label}}</td>
            <td>{{s.shortlabel}}</td>
          </tr>
        % end
      </table>
      <p />
      <table id="subchannels" class="w3-card-4 w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>Sub channel</th>
          <th>Type</th>
          <th>Input file</th>
          <th>Bit rate (Kbps)</th>
        </tr>
        % for s in subchannels:
          <tr>
            <td>{{s.name}}</td>
            <td>{{s.type}}</td>
            <td>{{s.inputfile}}</td>
            <td>{{s.bitrate}}</td>
          </tr>
        % end
      </table>
      <p />
      <table id="components" class="w3-card-4 w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>Component</th>
          <th>Label</th>
          <th>Short label</th>
          <th>Service</th>
          <th>Sub-channel</th>
          <th>Fig type</th>
        </tr>
        % for s in components:
          <tr>
            <td>{{s.name}}</td>
            <td>{{s.label}}</td>
            <td>{{s.shortlabel}}</td>
            <td>{{s.service}}</td>
            <td>{{s.subchannel}}</td>
            <td>{{s.figtype}}</td>
          </tr>
        % end
      </table>
      <p />
      <ul id="rcmodules" class="w3-card w3-ul">
        <li class="w3-blue-grey"><h3>RC Modules</h3></li>
        % for m in rcmodules:
          <li class="w3-light-grey"><b>{{m.name}}</b>
            <ul class="w3-ul">
              % for p in m.parameters:
                <li class="w3-white"><a href="/rc/{{m.name}}/{{p.param}}" class="w3-hover-blue-grey">{{p.param}}</a> : {{p.value}}</li>
              % end
            </ul>
          </li>
        % end
      </ul>
  </div>

  </body>
</html>