(ns editor.cubemap
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap]
           [com.jogamp.opengl GL GL2]
           [editor.types AABB]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(def cubemap-icon "icons/32/Icons_23-Cubemap.png")

(vtx/defvertex normal-vtx
  (vec3 position)
  (vec3 normal))

(def ^:private sphere-lats 32)
(def ^:private sphere-longs 64)

(def unit-sphere
  (let [vbuf (->normal-vtx (* 6 (* sphere-lats sphere-longs)))]
    (doseq [face (geom/unit-sphere-pos-nrm sphere-lats sphere-longs)
            v    face]
      (conj! vbuf v))
    (persistent! vbuf)))

(shader/defshader pos-norm-vert
  (uniform mat4 world)
  (attribute vec3 position)
  (attribute vec3 normal)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vNormal (normalize (* (mat3 (.xyz (nth world 0)) (.xyz (nth world 1)) (.xyz (nth world 2))) normal)))
    (setq vWorld (.xyz (* world (vec4 position 1.0))))
    (setq gl_Position (* gl_ModelViewProjectionMatrix (vec4 position 1.0)))))

(shader/defshader pos-norm-frag
  (uniform vec3 cameraPosition)
  (uniform samplerCube envMap)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vec3 camToV (normalize (- vWorld cameraPosition)))
    (setq vec3 refl (reflect camToV vNormal))
    (setq gl_FragColor (textureCube envMap refl))))

; TODO - macro of this
(def cubemap-shader (shader/make-shader ::cubemap-shader pos-norm-vert pos-norm-frag))

(defn render-cubemap
  [^GL2 gl render-args camera gpu-texture vertex-binding]
  (gl/with-gl-bindings gl render-args [gpu-texture cubemap-shader vertex-binding]
    (shader/set-uniform cubemap-shader gl "world" geom/Identity4d)
    (shader/set-uniform cubemap-shader gl "cameraPosition" (types/position camera))
    (shader/set-uniform cubemap-shader gl "envMap" 0)
    (gl/gl-enable gl GL/GL_CULL_FACE)
    (gl/gl-cull-face gl GL/GL_BACK)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* sphere-lats sphere-longs)))
    (gl/gl-disable gl GL/GL_CULL_FACE)))

(g/defnk produce-save-value [right left top bottom front back]
  {:right (resource/resource->proj-path right)
   :left (resource/resource->proj-path left)
   :top (resource/resource->proj-path top)
   :bottom (resource/resource->proj-path bottom)
   :front (resource/resource->proj-path front)
   :back (resource/resource->proj-path back)})

(g/defnk produce-scene
  [_node-id aabb gpu-texture]
  (let [vertex-binding (vtx/use-with _node-id unit-sphere cubemap-shader)]
    {:node-id    _node-id
     :aabb       aabb
     :renderable {:render-fn (fn [gl render-args _renderables _count]
                               (let [camera (:camera render-args)]
                                 (render-cubemap gl render-args camera gpu-texture vertex-binding)))
                  :passes [pass/transparent]}}))

(defn- cubemap-side-setter [resource-label image-label size-label]
  (fn [evaluation-context self old-value new-value]
    (project/resource-setter self old-value new-value
                             [:resource resource-label]
                             [:content image-label]
                             [:size size-label])))

(defn- build-cubemap [resource dep-resources user-data]
  (let [{:keys [images texture-profile compress?]} user-data
        texture-images (tex-gen/make-cubemap-texture-images images texture-profile compress?)
        cubemap-texture-image (tex-gen/assemble-cubemap-texture-images texture-images)]
    {:resource resource
     :content (protobuf/pb->bytes cubemap-texture-image)}))

(def ^:private cubemap-dir->property
  {:px :right
   :nx :left
   :py :top
   :ny :bottom
   :pz :front
   :nz :back})

(defn- cubemap-images-missing-error [node-id cubemap-image-resources]
  (when-let [error (some (fn [[dir image-resource]]
                           (when-let [message (validation/prop-resource-missing? image-resource (properties/keyword->name (cubemap-dir->property dir)))]
                             [dir message]))
                         cubemap-image-resources)]
    (let [[dir message] error]
      (g/->error node-id (cubemap-dir->property dir) :fatal nil message))))

(defn- size->width-x-height [size]
  (format "%sx%s" (:width size) (:height size)))

(defn- cubemap-image-sizes-error [node-id cubemap-image-sizes]
  (let [sizes (vals cubemap-image-sizes)]
    (when (and (every? some? sizes)
               (not (apply = sizes)))
      (let [message (apply format
                           "Cubemap image sizes differ:\n%s = %s\n%s = %s\n%s = %s\n%s = %s\n%s = %s\n%s = %s"
                           (mapcat (fn [dir] [(properties/keyword->name (cubemap-dir->property dir)) (size->width-x-height (dir cubemap-image-sizes))])
                                   [:px :nx :py :ny :pz :nz]))]
        (g/->error node-id nil :fatal nil message)))))

(g/defnk produce-build-targets [_node-id resource cubemap-images cubemap-image-resources cubemap-image-sizes texture-profile build-settings]
  (g/precluding-errors
    [(cubemap-images-missing-error _node-id cubemap-image-resources)
     (cubemap-image-sizes-error _node-id cubemap-image-sizes)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-cubemap
      :user-data {:images cubemap-images
                  :texture-profile texture-profile
                  :compress? (:compress? build-settings false)}}]))

(defmacro cubemap-side-error [property]
  (let [prop-kw (keyword property)
        prop-name (properties/keyword->name prop-kw)]
    `(g/fnk [~'_node-id ~property ~'cubemap-image-sizes]
            (or (validation/prop-error :fatal ~'_node-id ~prop-kw validation/prop-resource-missing? ~property ~prop-name)
                (cubemap-image-sizes-error ~'_node-id ~'cubemap-image-sizes)))))

(g/defnode CubemapNode
  (inherits resource-node/ResourceNode)
  (inherits scene/SceneNode)

  (input build-settings g/Any)
  (input texture-profiles g/Any)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (property right resource/Resource
            (value (gu/passthrough right-resource))
            (set (cubemap-side-setter :right-resource :right-image :right-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error right)))
  (property left resource/Resource
            (value (gu/passthrough left-resource))
            (set (cubemap-side-setter :left-resource :left-image :left-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error left)))
  (property top resource/Resource
            (value (gu/passthrough top-resource))
            (set (cubemap-side-setter :top-resource :top-image :top-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error top)))
  (property bottom resource/Resource
            (value (gu/passthrough bottom-resource))
            (set (cubemap-side-setter :bottom-resource :bottom-image :bottom-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error bottom)))
  (property front resource/Resource
            (value (gu/passthrough front-resource))
            (set (cubemap-side-setter :front-resource :front-image :front-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error front)))
  (property back resource/Resource
            (value (gu/passthrough back-resource))
            (set (cubemap-side-setter :back-resource :back-image :back-size))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext image/exts}))
            (dynamic error (cubemap-side-error back)))

  (input right-resource  resource/Resource)
  (input left-resource   resource/Resource)
  (input top-resource    resource/Resource)
  (input bottom-resource resource/Resource)
  (input front-resource  resource/Resource)
  (input back-resource   resource/Resource)

  (input right-image  BufferedImage)
  (input left-image   BufferedImage)
  (input top-image    BufferedImage)
  (input bottom-image BufferedImage)
  (input front-image  BufferedImage)
  (input back-image   BufferedImage)

  (input right-size  g/Any)
  (input left-size   g/Any)
  (input top-size    g/Any)
  (input bottom-size g/Any)
  (input front-size  g/Any)
  (input back-size   g/Any)

  (output cubemap-image-resources g/Any (g/fnk [right-resource left-resource top-resource bottom-resource front-resource back-resource]
                                          {:px right-resource :nx left-resource :py top-resource :ny bottom-resource :pz front-resource :nz back-resource}))

  (output cubemap-images g/Any (g/fnk [right-image left-image top-image bottom-image front-image back-image]
                                 {:px right-image :nx left-image :py top-image :ny bottom-image :pz front-image :nz back-image}))

  (output cubemap-image-sizes g/Any (g/fnk [right-size left-size top-size bottom-size front-size back-size]
                                      {:px right-size :nx left-size :py top-size :ny bottom-size :pz front-size :nz back-size}))

  (output gpu-texture g/Any :cached (g/fnk [_node-id cubemap-images cubemap-image-resources cubemap-image-sizes texture-profile]
                                      (g/precluding-errors
                                        [(cubemap-images-missing-error _node-id cubemap-image-resources)
                                         (cubemap-image-sizes-error _node-id cubemap-image-sizes)]
                                        (let [cubemap-texture-datas (texture/make-preview-cubemap-texture-datas cubemap-images texture-profile)]
                                          (texture/cubemap-texture-datas->gpu-texture _node-id cubemap-texture-datas)))))

  (output gpu-texture-generator g/Any (g/fnk [gpu-texture] (image/make-variant-gpu-texture-generator gpu-texture)))

  (output build-targets g/Any :cached produce-build-targets)

  (output transform-properties g/Any scene/produce-no-transform-properties)
  (output save-value  g/Any :cached produce-save-value)
  (output aabb        AABB  :cached (g/constantly geom/unit-bounding-box))
  (output scene       g/Any :cached produce-scene))

(defn load-cubemap [project self resource cubemap-message]
  (concat 
    (for [[side input] cubemap-message
          :let [image-resource (workspace/resolve-resource resource input)]]
      (g/set-property self side image-resource))
    (g/connect project :build-settings self :build-settings)
    (g/connect project :texture-profiles self :texture-profiles)))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "cubemap"
    :label "Cubemap"
    :build-ext "texturec"
    :node-type CubemapNode
    :ddf-type Graphics$Cubemap
    :load-fn load-cubemap
    :icon cubemap-icon
    :view-types [:scene :text]))
