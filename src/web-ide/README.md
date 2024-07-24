# TinySPARQL web IDE
Web IDE for testing SPARQL queries against arbitary TinySPARQL endpoints.

## Getting started
This website uses a few JS packages that we manage using npm, so please install that on your machine before you start developing. 
```
# to install dependencies
npm i

# run dev server with hot module replacement
npm run dev
# in another window/in the background turn on backend server when testing full functionalities

# run bundler to see pretty version of bundled files (in dev directory)
npx webpack

# run bundler to create production build (in dist directory)
npm run build

```

## Building with Meson
The static frontend files generated here will be converted into GResource bundles during the meson build process. The GResource bundling is automatic, but the file generation, since it is based on webpack, needs to be done manually. This can be done in the following 2 ways:

```
# option 1 - npm run build manually before running meson build scripts
# from web-ide directory
npm run build 
cd ../..
meson compile -C <builddir>

# option 2 - enable meson option for automated webpack compilation
# from root directory
meson configure -Dwebpack=true <builddir>
meson compile -C <builddir>

```

Note that the dist directory must be pushed to git to ensure developers who aren't involved with the web ide don't need to install all the npm stuff to build the project.