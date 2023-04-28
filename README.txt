Aryan Dornala
ad1496

I am submitting by myself.

Implementation choice: Concurrent games

Test Plan:

    (a) - What properties your game must have for you to consider it correct

            - validation of user input
            - appropriate responses sent by server
            - ability to handle multiple concurrent games 
            - once a game starts, only the appropriate player can make moves
            - detect if a game is over
            - users can only move to valid spots on the board
            - ability to listen to incoming connections while simultaneously running games

    (b) - How you intend to check that your code has these properties

            1. Start up the server and then connect multiple clients to the server. 
            2. Run a sequence of commands on the clients to test the properties listed in part (a).
            3. Make sure each client gets the proper response from the server.
    
    (d) - Step by step explanation of how to test

            - Ensure that you run:
                ~ make ttt
                ~ make ttts
            
            Testing procedure:
               1. Start server: ./ttts 15000
               2. Connect two clients to the server(client 1, client 2): ./ttt ilab.cs.rutgers.edu 15000
               3. Test input validation:
                  FORMAT: client #: input : expected response
                     client 1: PLAY|3|a| : Should get invalid from server since bytes don't match.
                     client 1: PLAY|2|a| : Should get WAIT from server.
                     client 2: test : Should get INVD from server since it is not a valid input.
                        you can test other invalid inputs as well such as MOVE, RSGN, and DRAW.
                     client 2: PLAY|2|a| : Should get INVD from server since player named 'a' already exists.
                     client 2: PLAY|2|b| : Should get WAIT from server.
                        client 1 and client 2 should now both receive a BEGN message to indicate that a game has started.
                        
               4. Connect two  more clients to the server(client 3, client 4): ./ttt ilab.cs.rutgers.edu 15000
               5. Test concurrent games:
                     client 3: PLAY|2|b| : Should get INVD from server since player named 'b' exists and is in a game.
                     client 3: PLAY|2|c| : Should get WAIT from server.
                     client 4: PLAY|2|d| : Should get WAIT from server.
                        client 3 and client 4 should now both receive a BEGN message to indicate that a game has started.
               6. Test appropriate turns:
                     client 1: test input: Should get INVD from the server indicating that the inputs for that player are being read.
                     client 2: test input: Should get no response from the server since player 2 is not reading input.
               6. Test MOVE:
                     client 1: MOVE|6|X|2,2| : Should get MOVD from the server with the state of the board.
                        Same message should get send to client 2.
                     client 2: MOVE|6|X|1,1| : Should get INVD from the server since the player should be 'O'.
                     client 2: MOVE|6|O|1,4| : Should get INVD from the server since the coordinate is out of bounds.
                     client 2: MOVE|6|O|2,2| : Should get INVD from the server since there is already a piece on that coordinate.
                     client 2: MOVE|6|O|1,1| : Should get MOVD from the server with the state of the board.
                        Same message should get send to client 1.
               7. Test DRAW:
                     client 3: DRAW|2|A| : Should get INVD from server since a draw can't be accepted if there is not draw suggested.
                     client 3: DRAW|2|D| : Should get INVD from server since a draw can't be denied if there is not draw suggested.
                     client 3: DRAW|2|S| : Should get no response from the server but instead send a response to client 4: 'DRAW|2|S|'
                        indicating that a draw has been suggested.
                     client 4: DRAW|2|R| : Should get no response from the server but instead send a response to client 3: 'DRAW|2|R|'
                        indicating that a draw has been rejected.
                     client 3: DRAW|2|S| : Should get no response from the server but instead send a response to client 4: 'DRAW|2|S|'
                        indicating that a draw has been suggested.
                     client 4: DRAW|2|A| : Should get OVER from the server with outcome 'D' since the game has ended as a draw. 
                        Client 3 should also receive the same message from the server.
                        Game is now over for client 3 and client 4, which means inputs are not read and their threads terminate while
                        also freeing up used memory.
               8. Test game over:
                     client 1: MOVE|6|X|1,2| : Should get MOVD from the server with the state of the board.
                        Same message should get send to client 2.
                     client 2: MOVE|6|X|2,1| : Should get MOVD from the server with the state of the board.
                        Same message should get send to client 1.
                     client 1: MOVE|6|X|3,2| : Should get MOVD from the server with the state of the board.
                        Same message should get send to client 2. Additionally, client 1(player X) has won since they got 3 in a row,
                        which means client 1 and client 2 should get OVER from the server stating that client 1 has won. 
                        Client 1 should also have outcome 'W' since they won, while client 2 should have outcome 'L' since they lost.
                        Game is now over.
               9. Connect two  more clients to the server (client 5, client 6): ./ttt ilab.cs.rutgers.edu 15000
               10. Test RSGN:
                     client 5: PLAY|2|a| : Should get WAIT from server since player 'a's game is over, removing that name from the list of players.
                     client 6: PLAY|2|b| : Should get WAIT from server since player 'b's game is over, removing that name from the list of players.
                        client 5 and client 6 should now both receive a BEGN message to indicate that a game has started.
                     client 5: RSGN|0| : Should get OVER from server with outcome 'L' since client 5 has resigned.
                        Client 6 should get the same message with outcome 'W' since the other player has resigned.
               11. Make sure the server is appropriately logging each client's input to STDOUT.
