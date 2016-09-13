require 'aruba/cucumber'
require 'concurrent/executors'
# require 'eventmachine'
require 'em-rubyserial'

em_thread_exec = Concurrent::SingleThreadExecutor.new

Before do |scenario|
  # Start event loop in separate thread
  em_started = Concurrent::Event.new
  em_thread_exec.post do
    EM.run do
      em_started.set
    end
  end
  em_started.wait
  @current_scenario = scenario
end

After do |scenario|
  Particle::Usb.instance.reset
  Particle::Serial.instance.reset
  # Stop event loop
  EM.stop_event_loop
end
