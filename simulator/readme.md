# OpenEVSE WiFi Simulator

Node web server app to simulate an OpenEVSE WiFi gateway, usually running on ESP8266 communicating with openevse controller via serial RAPI API

*This simulator is for demo/testing, to get a feel for the interface. Not all features have been implemented fully.*

## Requirements

```
sudo apt-get intall node nodejs npm
```

Tested with `npm V5.6.0` and nodejs `v9.5.0`.

If a new version of nodejs is not available for your distribution you may need to update, [see nodejs install page](https://nodejs.org/en/download/package-manager/#debian-and-ubuntu-based-linux-distributions).



## Setup

```
cd simulator
npm install
node app.js --port 3000
```

Then point your browser at http://localhost:3000/

Depending on your npm setup you may need to install the following:

```
npm install body-parser
npm install express
npm install
```


**Tip**
The OpenEVSE WiFi HTML/JS/CSS can be 'compiled' without building the full firmware using the command:

```
pio run -t buildfs
```

## Run as a service

### Using systemd

`sudo cp openevse.service /etc/systemd/system/openevse.service`

Edit service file to specify correct path to match installation location

`sudo nano /etc/systemd/system/openevse.service`

Run at startup:

```
sudo systemctl daemon-reload
sudo systemctl enable openevse.service
```

### Using PM2

```
sudo npm install -g pm2
pm2 start app.js
```

For status:

```
pm2 info app
pm2 list
pm2 restart app
mp2 stop app
```


## Serve via apache


Install apache `mod-proxy` module then enable it:

```
sudo apt-get install libapache2-mod-proxy-html
sudo a2enmod proxy
sudo a2enmod proxy_http
sudo a2enmod rewrite
```

copy `example-openevse-apache.conf` to `/etc/apache2/sites-available` making the relevant changes for your server then enable the site using `a2ensite`. e.g.

```
sudo cp example-openevse-apache.conf /etc/apache2/sites-available/openevse.conf
sudo a2ensite openevse
```

Create log files, this step may not be needed but it's a good idea to check the permissions.

```
sudo touch /var/log/apache2/openevse_error.log
sudo touch /var/log/apache2/openevse_access.log
sudo service restart apache2
```
