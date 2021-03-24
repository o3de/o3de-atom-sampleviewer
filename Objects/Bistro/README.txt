Amazon Lumberyard Bistro

Vertex counts:
- Interior: 814,435
- Exterior: 2,965,809

Textures (compressed .DDS) designed for GGX-based metal-rough PBR material system with the following convention: 
- BaseColor
	RGB channels: BaseColor value
	Alpha channel: Opacity

- Specular:
	Red channel: Occlusion
	Green channel: Roughness
	Blue channel: Metalness

- Normal (DirectX)

Also included:
- Spherical reflection HDR maps for interior and exterior 
- Environment HDR map for exterior from https://hdrihaven.com
- Falcor scene files (Bistro_Interior.fscene, Bistro_Exterior.fscene) to be used with Falcor's forward rendering demo. Contains lighting and camera information.

How to cite use of this asset:

  @misc{ORCAAmazonBistro,
   title = {Amazon Lumberyard Bistro, Open Research Content Archive (ORCA)},
   author = {Amazon Lumberyard},
   year = {2017},
   month = {July},
   note = {\small \texttt{http://developer.nvidia.com/orca/amazon-lumberyard-bistro}},
   url = {http://developer.nvidia.com/orca/amazon-lumberyard-bistro}
}