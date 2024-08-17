import globals from "globals";
import pluginJs from "@eslint/js";
import tseslint from "typescript-eslint";


export default [
  {
    languageOptions: { 
      globals: globals.browser,
      parser: "@typescript-eslint/parser",
      parserOptions: {
        ecmaVersion: 2020,
        sourceType: "module",
        project: "./tsconfig.json",
      } 
    }
  },
  pluginJs.configs.recommended,
  ...tseslint.configs.recommended,
  ...tseslint.configs.stylistic
];

// export default [
//   {
//     files: ["src/*.ts"],
//     languageOptions: {
//       parser: "@typescript-eslint/parser",
//       parserOptions: {
//         ecmaVersion: 2020,
//         sourceType: "module",
//         project: "./tsconfig.json",
//       }
//     },
//     plugins: ["@typescript-eslint"],
//     extends: ["eslint:recommended", "plugin:@typescript-eslint/recommended"],
//     rules: {
//       "no-console": "off",
//       "no-continue": "off",
//       "@typescript-eslint/indent": [
//         "error",
//         2
//       ],
//       "object-curly-newline": [
//         "error",
//         {
//           "ObjectExpression": { "multiline": true, "minProperties": 3 },
//           "ObjectPattern": { "multiline": true },
//           "ImportDeclaration": { "multiline": true, "minProperties": 4 },
//           "ExportDeclaration": "never"
//         }
//       ],
//       "max-len": [
//         "error",
//         {
//           "code": 140,
//           "ignoreStrings": true
//         }
//       ],
//       "no-await-in-loop": "off",
//       "no-restricted-syntax": [
//         "error",
//         {
//           "selector": "ForInStatement",
//           "message": "for..in loops iterate over the entire prototype chain, which is virtually never what you want. Use Object.{keys,values,entries}, and iterate over the resulting array."
//         },
//         {
//           "selector": "LabeledStatement",
//           "message": "Labels are a form of GOTO; using them makes code confusing and hard to maintain and understand."
//         },
//         {
//           "selector": "WithStatement",
//           "message": "`with` is disallowed in strict mode because it makes code impossible to predict and optimize."
//         }
//       ],
//       "implicit-arrow-linebreak": "off",
//       "import/no-extraneous-dependencies": "off",
//       "@typescript-eslint/no-unused-vars": [
//         "error",
//         {
//           "argsIgnorePattern": "^_"
//         }
//       ],
//       "@typescript-eslint/quotes": ["error", "double"]
//     },
  
//     env: {
//       "browser": true,
//       "es2017": true
//     }
//   }
  
// ];