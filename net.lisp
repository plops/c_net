(defpackage net (:use :cl :sb-bsd-sockets))
(in-package net)

(defparameter *host* (make-inet-address "127.0.0.1"))
(defparameter nc-stream
  (socket-make-stream 
   (let ((s (make-inet-socket :stream :tcp)))
     (socket-connect s *host* 1234)
     s)
   :element-type '(unsigned-byte 8)
   :input t
   :output nil
   :buffering :none))

(defparameter q (make-array (* 1024 1024)))

(read-sequence q nc-stream)

