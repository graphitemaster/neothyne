# World format

The world format for Neothyne is represented as a subset of the OBJ model format.

### Vertex
A vertex beigns with 'v' followed by three single-precision floating-point values
separated by spaces. Example
`
v [x] [y] [z]
`

### Texture Coordinate
A texture coordinate begins with 'vt' followed by two single-precision floating-
point values separated by spaces. Example
```
vt [u] [v]
```

### Face
A face begins with 'f' followed by three tuples separated by spaces.
The tuple format begins with the vertex index as an integer followed by
a forward-slash('/') following with a texture coordinate index. Example of
the tuple format
```
[vertexIndex]/[texCoordIndex]
```
Example of a face
```
f [vertexIndex]/[texCoordIndex] [vertexIndex]/[texCoordIndex] [vertexIndex]/[texCoordIndex]
```

### Entity
An entity begins with 'ent' followed by a single integer followed by seven
single-precison floating-point values separated by spaces. Example
```
ent [id] [origin:x] [origin:y] [origin:z] [rot:x] [rot:y] [rot:z] [rot:w]
```

### Texture
A texture begins with 'tex' followed by the relative path to a texture file.
```
tex [textur_path/file.ext]
```
When set, a texture will be used for all faces specified after it.

### Notes
There are no vertex normals or materials to be specified for worlds in Neothyne.

A small example

```
v 0.0 0.0 0.0
v 0.0 0.0 1.0
v 0.0 1.0 0.0
v 0.0 1.0 1.0
v 1.0 0.0 0.0
v 1.0 0.0 1.0
v 1.0 1.0 0.0
v 1.0 1.0 1.0

tex a.jpg
f 1/1 2/2 3/3
f 3/3 2/2 4/4
tex b.jpg
f 3/1 4/2 5/3
f 5/3 4/2 6/4
tex c.jpg
f 5/4 6/3 7/2
f 7/2 6/3 8/1
tex d.jpg
f 7/1 8/2 1/3
f 1/3 8/2 2/4
tex e.jpg
f 2/1 8/2 4/3
f 4/3 8/2 6/4
tex f.jpg
f 7/1 1/2 5/3
f 5/3 1/2 3/4

ent 0 0.5 0.5 0.5 0.0 0.0 0.0 0.0
```

The following defines a textured cube with an entity in the middle of it.
