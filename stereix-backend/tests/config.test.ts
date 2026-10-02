jest.mock('dotenv', () => ({
  __esModule: true,
  default: { config: jest.fn() },
}));

const originalEnv = process.env;

beforeEach(() => {
  jest.resetModules();
  process.env = { ...originalEnv, NODE_ENV: 'production' };
  delete process.env.JWT_SECRET;
});

afterEach(() => {
  process.env = originalEnv;
});

function loadConfig(): typeof import('../src/common/config.js').config {
  return (require('../src/common/config.js') as typeof import('../src/common/config.js')).config;
}

describe('production JWT configuration', () => {
  it.each([
    ['missing', undefined, /secure, custom JWT_SECRET/],
    ['the default fallback', 'nexvr-enterprise-jwt-super-secret-key-change-in-prod', /secure, custom JWT_SECRET/],
    ['too short', 'short-secret', /at least 32 characters/],
  ])('rejects %s JWT_SECRET', (_case, secret, message) => {
    if (secret) process.env.JWT_SECRET = secret;
    expect(() => loadConfig()).toThrow(message);
  });

  it('accepts a custom secret of sufficient length', () => {
    const secret = 'test-only-custom-jwt-secret-at-least-32-chars';
    process.env.JWT_SECRET = secret;

    expect(loadConfig()).toMatchObject({ NODE_ENV: 'production', JWT_SECRET: secret });
  });
});
