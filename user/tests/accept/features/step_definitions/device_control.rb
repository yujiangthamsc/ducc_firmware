When(/^I reset the device$/) do
  Particle::Util.exec("device_ctl reset")
  sleep(1.5) # Give the device some time to boot into the application
end
