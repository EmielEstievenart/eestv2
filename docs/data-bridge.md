Please implement a data bridge. 
It should do the following things: 
Take command line parameters: 
- v or -vv or -vvv for different verbosity levels
- pass=XXX: XXX should be the friendly string used during discovery. Instead of port numbers, we use this and use the udp discovery sockets to establish a connection. 
- Every instance should try to connect to other instances, but also accept connections. Naturaly this entails that when it receives a connection it will determine if that connection is already established. Be it by connecting or receiving a connection. Take care, this means that when connecting, a filter mechanism is required. Use the original passphrase in combo with the port and Ip address to determine a duplicate connection. 
- A connection can be established by using the DiscoveryServer and Client that uses UDP broadcasting. Choose a default port for UDP broadcasting, but also make it optional via a command line param. When you receive a broadcasted message that contains the passphrase, you may reply with the passphrase and the IP address and port on which you accept connections for that service. The same happens the other way around. 
- You should always be on the lookout for new connections via the discovery protocol. 
- Once a connection is established, all data from all connections is forwarded to the other connections. 
- Use a single io_context from boost ASIO to manage your connections. 
- Use the networking code I already created. TcpServer, TcpClient, TcpConnection, Discoverable, DiscoveryServer, ... and so on. 
- Use the serializer multiplexor and de-multiplexor to implement a way to send data back and forth over the data-bridge participants. 
- One of the things that must be transmitted is a ping struct, only containing 1 byte, a rolling counter. The ping must be send every second. And indicates a live connection. If no ping is transferd, consider the connection dead and be open to receive a new connection from that host, or try re-connecting to it. 
- The end goal here is that I can hook it up to a CAN-bus and transfer can-messages back and forth. Let's do that by alreayd creating a notion of a can-message struct. 
The struct is just:
 8 bytes payload, 
 a 32bit header, 
 a can-bus number 
 a number of bytes used in the payload. 
 Make sure I can use the CAN-bus interface to implement a concrete access to and from the can bus. 
- Data-bridge is implemented as a library, that means you can use Google tests to completely test your implementation. 


