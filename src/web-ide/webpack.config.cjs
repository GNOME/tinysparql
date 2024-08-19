const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');

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
      title: 'TinySPARQL web-IDE',
      template: './src/index.html',
    }),
    new MiniCssExtractPlugin({
      filename: 'styles.css'
    }),
    new HtmlWebpackPlugin({
      title: '404 Not Found',
      template: './src/404.html',
      filename: '404.html'
    })
  ],
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: 'ts-loader',
      },
      {
        test: /\.(png|svg|jpg|jpeg|gif|ico)$/i,
        type: 'asset/resource',
      },
      {
        test: /\.scss$/,
        use: [
            MiniCssExtractPlugin.loader,
            {
              loader: 'css-loader'
            },
            {
              loader: 'sass-loader',
              options: {
                sourceMap: true
              }
            }
          ]
      }
    ],
  },
  resolve: {
    extensions: ['.tsx', '.ts', '.js'],
  },
};