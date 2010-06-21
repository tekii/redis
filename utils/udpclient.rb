# This is just a start of an UDP client for Redis
# For now I'm using this just for testing, but may evolve into a real client
# to be merged in Redis-rb.

require 'socket'

class UDPRedisClient
    def initialize(host,port,opt={})
        @sock = UDPSocket.new
        @sock.connect(host,port)
        @opt = opt
        @reqid = 0
    end

    def createRequestPacket(reqid,dbid,argv,opt)
        flags = 0
        flags |= 0x01 if opt[:noreply]
        flags |= 0x02 if opt[:noack]
        if opt[:auth]
            flags |= 0x03
            argv = [opt[:auth]]+argv
        end
        payload = ""
        argv.each{|x|
            x = x.to_s
            payload << [x.length].pack("n")
            payload << x
        }
        packet = [reqid,1,flags,payload.length,argv.length,dbid].pack("NCCnnn")
        packet << payload
    end

    def sendRequestPacket(dbid,argv,opt={})
        @reqid += 1
        @sock.send(createRequestPacket(@reqid,dbid,argv,opt.merge(@opt)),0)
        return @reqid
    end

    def recvReplyPacket()
        return @sock.recvfrom(65536)
    end
end

#udp = UDPRedisClient.new("192.168.1.105",6379)
udp = UDPRedisClient.new("127.0.0.1",6379)
#udp.sendRequestPacket(0,["ping"])
#exit
(0..1000000).each{|n|
    udp.sendRequestPacket(0,["ping"])
#    udp.sendRequestPacket(0,["set","foo#{n}",10])
    udp.recvReplyPacket
}
# puts udp.recvReplyPacket.inspect
