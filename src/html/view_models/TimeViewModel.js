/* global ko */
/* exported TimeViewModel */

function TimeViewModel(openevse)
{
  "use strict";
  var self = this;

  function addZero(val) {
    return (val < 10 ? "0" : "") + val;
  }
  function startTimeUpdate() {
    timeUpdateTimeout = setInterval(function () {
      if(self.automaticTime()) {
        self.nowTimedate(new Date(self.evseTimedate().getTime() + ((new Date()) - self.localTimedate())));
      }
      if(openevse.isCharging()) {
        self.elapsedNow(new Date((openevse.status.elapsed() * 1000) + ((new Date()) - self.elapsedLocal())));
      }
    }, 1000);
  }
  function stopTimeUpdate() {
    if(null !== timeUpdateTimeout) {
      clearInterval(timeUpdateTimeout);
      timeUpdateTimeout = null;
    }
  }

  self.evseTimedate = ko.observable(new Date());
  self.localTimedate = ko.observable(new Date());
  self.nowTimedate = ko.observable(null);

  self.elapsedNow = ko.observable(new Date(0));
  self.elapsedLocal = ko.observable(new Date());

  self.date = ko.pureComputed({
    read: function () {
      if(null === self.nowTimedate()) {
        return "";
      }

      return self.nowTimedate().toISOString().split("T")[0];
    },
    write: function (val) {
      self.evseTimedate(new Date(val));
      self.localTimedate(new Date());
    }});
  self.time = ko.pureComputed({
    read: function () {
      if(null === self.nowTimedate()) {
        return "--:--:--";
      }
      var dt = self.nowTimedate();
      return addZero(dt.getHours())+":"+addZero(dt.getMinutes())+":"+addZero(dt.getSeconds());
    },
    write: function (val) {
      var parts = val.split(":");
      var date = self.evseTimedate();
      date.setHours(parseInt(parts[0]));
      date.setMinutes(parseInt(parts[1]));
      self.evseTimedate(date);
      self.localTimedate(new Date());
    }});
  self.elapsed = ko.pureComputed(function () {
    if(null === self.nowTimedate()) {
      return "0:00:00";
    }
    var dt = self.elapsedNow();
    return addZero(dt.getUTCHours())+":"+addZero(dt.getMinutes())+":"+addZero(dt.getSeconds());
  });

  openevse.status.elapsed.subscribe(function (val) {
      self.elapsedNow(new Date(val * 1000));
      self.elapsedLocal(new Date());
  });

  var timeUpdateTimeout = null;
  self.automaticTime = ko.observable(true);
  self.setTime = function () {
    var newTime = self.automaticTime() ? new Date() : self.evseTimedate();
    // IMPROVE: set a few times and work out an average transmission delay, PID loop?
    openevse.openevse.time(self.timeUpdate, newTime);
  };

  self.timeUpdate = function (date) {
    stopTimeUpdate();
    self.evseTimedate(date);
    self.nowTimedate(date);
    self.localTimedate(new Date());
    startTimeUpdate();
  };
}
