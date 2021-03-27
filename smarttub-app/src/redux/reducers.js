import {
    SET_SETTING
} from './actionTypes';

const initialState = {
    settings: {}
};

export default function (state = initialState, action) {
    switch (action.type) {
        case SET_SETTING:
            return Object.assign({}, state, action.payload);
        default:
            return state;
    }
}