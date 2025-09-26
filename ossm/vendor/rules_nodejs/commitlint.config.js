module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    'header-max-length': [2, 'always', 120],
    'scope-enum': [
      2,
      'always',
      [
        'builtin',
        'runfiles',
        'worker',
        'docs',
      ],
    ],
  },
};
