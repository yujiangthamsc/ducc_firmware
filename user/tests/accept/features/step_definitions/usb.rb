require 'singleton'
require 'hex_string'

module Particle
  class Usb
    include Singleton

    DEFAULT_REQUEST = 80 # Reserved for Particle's USB requests
    DEFAULT_REQUEST_VALUE = 0
    DEFAULT_REQUEST_INDEX = 60 # USBRequestType::USB_REQUEST_TEST

    def initialize
      reset
    end

    def send_request(request: DEFAULT_REQUEST, value: DEFAULT_REQUEST_VALUE, index: DEFAULT_REQUEST_INDEX, out: false, data: '')
      data = data.to_hex_string(false)
      if !out # FIXME
        data = Util.exec("send_usb_req -r #{request} -v #{value} -i #{index} -x #{data}")
      else
        data = Util.exec("send_usb_req -r #{request} -v #{value} -i #{index} -d out -x #{data}")
      end
      @last_reply = data.to_byte_string
    end

    def take_reply
      raise "No reply data available" unless @last_reply
      @last_reply
    end

    def reset
      @last_reply = nil
    end
  end
end

usb = Particle::Usb.instance

# Step definitions
When(/^I send( host-to-device)? USB request(?: with index set to (\d+))?$/) do |out, index, data|
  index ||= Particle::Usb::DEFAULT_REQUEST_INDEX
  usb.send_request(index: index, data: data, out: !out.nil?)
end

Then(/^USB reply should (be|contain|match|start with|end with)$/) do |op, data|
  d = usb.take_reply
  Particle::Util.should(d, data, op)
end
