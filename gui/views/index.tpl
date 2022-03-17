<!DOCTYPE html>
<html>
  <head>
    <title>ODR-DabMux Configuration</title>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://www.w3schools.com/w3css/4/w3.css"> 
  </head>
  <body class="w3-container">
    <div class="w3-top w3-bar w3-blue-grey">
      <a href="#general" class="w3-bar-item w3-button">General Options</a>
      <a href="#servicelist" class="w3-bar-item w3-button">Services</a>
      <a href="#subchannels" class="w3-bar-item w3-button">Subchannels</a>
      <a href="#components" class="w3-bar-item w3-button">Components</a>
      <a href="#rcmodules" class="w3-bar-item w3-button">RC Modules</a>
    </div>
    <div id="general" class="w3-responsive w3-card-4">
      <br /><br />
      <table class="w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>General multiplex options</th>
          <th></th>
        </tr>
        <tr>
          <td>Number of frames to encode</td>
          <td>{{g.nbframes}}</td>
        </tr>
        <tr>
          <td>Statistics server port</td>
          <td>{{g.statsserverport}}</td>
        </tr>
        <tr>
          <td>Write SCCA field</td>
          <td>{{g.writescca}}</td>
        </tr>
        <tr>
          <td>Write TIST timestamp</td>
          <td>{{g.tist}}</td>
        </tr>
        <tr>
          <td>DAB mode</td>
          <td>{{g.dabmode}}</td>
        </tr>
        <tr>
          <td>Log to syslog</td>
          <td>{{g.syslog}}</td>
        </tr>
      </table>
    </div>
    <div id="servicelist" class="w3-responsive w3-card-4">
      <br /><br />
      <table class="w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>Service</th>
          <th>Id</th>
          <th>Label</th>
          <th>Short label</th>
        </tr>
        {% for s in services %}
          <tr>
            <td>{{s.name}}</td>
            <td>{{s.id}}</td>
            <td>{{s.label}}</td>
            <td>{{s.shortlabel}}</td>
          </tr>
        {% endfor %}
      </table>
    </div>
    <div id="subchannels" class="w3-responsive w3-card-4">
      <br /><br />
      <table class="w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>Sub channel</th>
          <th>Type</th>
          <th>Input file</th>
          <th>Bit rate (Kbps)</th>
        </tr>
        {% for s in subchannels %}
          <tr>
            <td>{{s.name}}</td>
            <td>{{s.type}}</td>
            <td>{{s.inputfile}}</td>
            <td>{{s.bitrate}}</td>
          </tr>
        {% endfor %}
      </table>
    </div>
    <div id="components" class="w3-responsive w3-card-4">
      <br /><br />
      <table class="w3-table w3-striped w3-bordered">
        <tr class="w3-blue-grey">
          <th>Component</th>
          <th>Label</th>
          <th>Short label</th>
          <th>Service</th>
          <th>Sub-channel</th>
          <th>Fig type</th>
        </tr>
        {% for s in components %}
          <tr>
            <td>{{s.name}}</td>
            <td>{{s.label}}</td>
            <td>{{s.shortlabel}}</td>
            <td>{{s.service}}</td>
            <td>{{s.subchannel}}</td>
            <td>{{s.figtype}}</td>
          </tr>
        {% endfor %}
      </table>
    </div>
    <div id="rcmodules" class="w3-responsive w3-card-4">
      <br /><br />
      <ul class="w3-ul">
        <li class="w3-blue-grey"><b>RC Modules</b></li>
        {% for m in rcmodules %}
          <li class="w3-light-grey"><b>{{m.name}}</b>
            <ul class="w3-ul">
              {% for p in m.parameters %}
                <li class="w3-white"><a href="/rc/{{m.name}}/{{p.param.decode()}}" class="w3-hover-blue-grey">{{p.param.decode()}}</a> : {{p.value.decode()}}</li>
              {% endfor %}
            </ul>
          </li>
        {% endfor %}
      </ul>
    </div>
  </div>

  </body>
</html>