(require :asdf)
(require :cffi)
(require :cells-gtk)
(defpackage :kielhorn.martin.cgtk-show
  (:use :cells :cl :cells-gtk :sb-bsd-sockets))
(in-package :kielhorn.martin.cgtk-show)

(defclass bild () ; can't call it image - clashes with something else in cgtk
  ((width :initarg :width :reader width)
   (height :initarg :height :reader height)
   (data :initarg :data :accessor data)))

(defun make-bild (width height &optional data)
  (let ((data (or data
		  (cffi:make-shareable-byte-vector 
		   (* width height)))))
    (make-instance 'bild
		   :width width
		   :height height
		   :data data)))

(defmodel my-app (gtk-app)
  ()
  (:default-initargs
      :md-name :martin-top
    :width (c-in 700)
    :height (c-in 450)
    :kids (c-in nil)))

(init-gtk)
(start-win 'my-app)

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

(defparameter *img* (make-bild 512 512))

#|
(close nc-stream)
|#

(defun draw-grid (&optional (w 100) (h 100))
;  (gl:line-width .7)
  (gl:color 1 1 1 .6)
  (gl:with-primitive :lines
    (loop for i from 0 below w by 20 do
	  (gl:vertex i 0) (gl:vertex i 210))
    (loop for i from 0 below h by 20 do
	  (gl:vertex 1 i) (gl:vertex 210 i))))

(defparameter *texture* nil)
(defun unload-tex ()
  (when *texture*
    (gl:delete-textures (list *texture*))
    (setf *texture* nil)))

(defun load-tex (img) 
  (unless *texture*
   (let ((texture (first (gl:gen-textures 1))))
     (gl:bind-texture :texture-2d texture)
     (gl:tex-parameter :texture-2d :texture-mag-filter :linear)
     (gl:tex-parameter :texture-2d :texture-min-filter :linear)
     (cffi:with-pointer-to-vector-data (addr (data img))
       (gl:tex-image-2d :texture-2d 0 :rgba
			(width img) (height img)
			0 :luminance :unsigned-byte addr))
     (setf *texture* texture))))

(defun update-tex (img)
  (gl:bind-texture :texture-2d *texture*)
       (cffi:with-pointer-to-vector-data (addr (data img))
	 (gl:tex-sub-image-2d :texture-2d 0 
			      0 0 
			      (width img) (height img)
			      :luminance :unsigned-byte addr)))

(defun upload-camera-image ()
    (read-sequence (data *img*) nc-stream)
    (update-tex *img*))

(defun threed-draw (self)
  ;(unload-tex)
  (load-tex *img*)
  (upload-camera-image)
  (gl:clear :color-buffer-bit :depth-buffer-bit)
  (gl:with-pushed-matrix
    (gl:translate 0 (* 9 (zpos self)) 0)
    (draw-grid (width self) (height self))

    (gl:enable :texture-2d)
    (gl:bind-texture :texture-2d *texture*)
    (gl:color 1 1 1)
    (gl:scale .5 .5 1)
    (gl:with-primitive :quads
      (gl:vertex 0 0) (gl:tex-coord 0 0)
      (gl:vertex 0 512) (gl:tex-coord 0 1)
      (gl:vertex 512 512) (gl:tex-coord 1 1)
      (gl:vertex 512 0) (gl:tex-coord 1 0))
    (gl:disable :texture-2d)

    (gl:with-primitive :lines
      (gl:line-width 2)
      (gl:color 1 0 0) (gl:vertex 50 0 0) (gl:vertex 0 0 0)
      (gl:color 0 1 0) (gl:vertex 0 50 0) (gl:vertex 0 0 0)
      (gl:color 0 0 1) (gl:vertex 0 0 50) (gl:vertex 0 0 0)))
;  (timeout-add 30 #'(lambda () (redraw (find-widget :threed))))
  )

(defmodel threed (gl-drawing-area)
  ((zpos :cell t :reader zpos :initarg :zpos :initform (c-in 0)))
  (:default-initargs
      :width (c-in 300)
    :height (c-in 300)
    :init #'(lambda (self)
	      (gl:enable :blend :depth-test :line-smooth)
	      (gl:blend-func :one :src-alpha)
	      (with-integrity (:change :adjust-widget-size)
		(setf (allocated-width self) (width self)
		      (allocated-height self) (height self))))
    :resize #'(lambda (self)
		(let ((w (allocated-width self))
		      (h (allocated-height self)))
		  (gl:viewport 0 0 w h)
		 (with-matrix-mode (:projection)
		   (gl:ortho 0 w h 0 -1 1))
		 (gl:matrix-mode :modelview)
		 (gl:load-identity)))
    :draw 'threed-draw))


(setf (kids (find-widget :martin-top))
      (list 
       (mk-vbox :fm-parent (find-widget :martin-top)
		:kids
		(kids-list? 
		 (mk-hscale :md-name :scale-zpos
			    :min -10
			    :max 10
			    :init 0
			    :step .05
			    )
		 (make-kid 'threed
			   :md-name :threed
			   :zpos (c? (progn
				       (redraw (find-widget :threed))
				       (widget-value :scale-zpos))))))))
#|
(progn
   (not-to-be (find-widget :threed))
   (setf (kids (find-widget :martin-top))
	 nil))
|#



