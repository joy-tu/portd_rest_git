###Overview

The lib which help to handle http server/client request and rest related parser.


###Scope


User program could use this libary to implement the rest server/client, and 
register the corresponding rest api to handle the request.


###Feature


http server to receive request.
http client to send request to another server.
rest parser and callback.


###Design Arch

[User program] => initial http server, register rest call back functions.

[http server] =receive request=> [rest parser] => [user call back]


###FOSS

nng(nanomsg-next-gen) / nng  

Version : 1.1.1  
Lisence Type : MIT  
Lisence Decleration :  
NNG is licensed under a liberal, and commercial friendly, MIT license.  
The goal to the license is to minimize friction in adoption, use,  
and contribution.  
Source Package download URI :  
https://github.com/nanomsg/nng  

parson / parson  
Version : none  
Lisence Type : MIT  
Lisence Decleration :  
Lightweight JSON library written in C.  
Source Package download URI :  
https://github.com/kgabis/parson  


###Source Instruction  
nanomsg-next-generation -- light-weight brokerless messaging   
https://nanomsg.github.io/nng/  

/http_server/nng/1.1.1 : the nng source code.  
/http_server/http_server_nng.c : the warper of the nng.  

/rest_parser/rest_parson.c : the rest api implemented base on parson.  
/rest_parser/parson/parson.c : the parson source code.  

/test/rest_client : the simple rest client for test.  
