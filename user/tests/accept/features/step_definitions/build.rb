Given(/^the application (.+)$/) do |app_dir|
  feature_path = File.dirname(@current_scenario.location.file)
  app_path = File.join(feature_path, app_dir)
  Particle::Util.exec("make_app #{app_path}")
  sleep(1.5) # Give the device some time to boot into the application
end
