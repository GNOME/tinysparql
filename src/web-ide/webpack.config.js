const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');

module.exports = {
  entry: './src/index.ts',
  mode: 'development',
  optimization: {
    usedExports: true
  },
  output: {
    filename: 'main.js',
    path: path.resolve(__dirname, 'dev'),
    clean: true
  },
  devtool: 'inline-source-map',
  devServer: {
    static: './dev'
  },
  plugins: [
    new HtmlWebpackPlugin({
        title: 'CodeMirror Demo',
        template: './src/index.html',
    })
  ],
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: 'ts-loader',
      },
    ],
  },
  resolve: {
    extensions: ['.tsx', '.ts', '.js'],
  },
};