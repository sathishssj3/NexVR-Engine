import { Router } from 'express';
import { AuthController } from './auth.controller.js';
import { authenticate } from './auth.middleware.js';

const router = Router();
const controller = new AuthController();

router.post('/register', controller.register);
router.post('/login', controller.login);
router.post('/refresh', controller.refresh);
router.post('/logout', controller.logout);
router.get('/me', authenticate, controller.me);

export const authRouter = router;
