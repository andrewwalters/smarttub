import {
    SET_SETTING
} from './actionTypes';

import axios from 'axios';

const url = '/api';

export const setSetting = (name, value) => dispatch => {
    const payload = {};
    payload[name] = value;
    const cmd = 'AT.' + name + '=' + value;
    dispatch({ type: SET_SETTING, payload });
    console.log('dispatch', name, value);
    return axios.get(url, { params: { q: cmd } }).then(res => {
        console.log('done', name, value, res);
    });
}