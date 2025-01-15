# Readme

* start acq X1 X2 X3 -------> X1 is PP in ns, X2 is PN in ns, X3 is PD in ns this starts the acquisitions syncing with the pulse.
* write dac X -------> X is the dac number I forgot the min and the max but it may be from 0 to 400.
* write mux XXXX ---> where XXXX is the word to be sent to the MAX14866.
* clear mux -- opens every switch
* set mux -- closes every switch
* read -------> read the obtained acquisitions (8000kpts).